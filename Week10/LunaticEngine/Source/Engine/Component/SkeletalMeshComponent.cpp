#include "Component/SkeletalMeshComponent.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetData.h"
#include "Asset/AssetFileSerializer.h"
#include "Collision/RayUtils.h"
#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	bool IsNonePath(const FString& Path) { return Path.empty() || Path == "None"; }

	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path ResolvedPath(FPaths::ToWide(Path));
		if (!ResolvedPath.is_absolute())
		{
			ResolvedPath = std::filesystem::path(FPaths::RootDir()) / ResolvedPath;
		}
		return ResolvedPath.lexically_normal();
	}

	constexpr float BoneArmatureMinLength = 0.001f;
	constexpr float BoneArmatureRadiusRatio = 0.05f;
	constexpr float BoneArmatureMinRadius = 0.004f;
	constexpr float BoneArmatureMaxRadius = 0.35f;
	constexpr float BoneArmatureBaseBias = 0.08f;
	constexpr float BoneJointRadiusRatio = 0.005f;

	struct FBoneArmatureFrame
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Axis = FVector(1.0f, 0.0f, 0.0f);
		FVector Right = FVector(0.0f, 1.0f, 0.0f);
		FVector Up = FVector(0.0f, 0.0f, 1.0f);
		float Length = 0.0f;
		float Radius = 1.0f;
	};

	float Determinant3x3(const FMatrix& Matrix)
	{
		return Matrix.M[0][0] * (Matrix.M[1][1] * Matrix.M[2][2] - Matrix.M[1][2] * Matrix.M[2][1]) - Matrix.M[0][1] * (Matrix.M[1][0] * Matrix.M[2][2] - Matrix
			.M[1][2] * Matrix.M[2][0]) + Matrix.M[0][2] * (Matrix.M[1][0] * Matrix.M[2][1] - Matrix.M[1][1] * Matrix.M[2][0]);
	}


	FVector AbsVector(const FVector& Value)
	{
		return FVector(std::fabs(Value.X), std::fabs(Value.Y), std::fabs(Value.Z));
	}

	FVector MirrorSignFromMatrix(const FMatrix& Matrix)
	{
		return Determinant3x3(Matrix) < 0.0f ? FVector(-1.0f, 1.0f, 1.0f) : FVector::OneVector;
	}

	struct FDecomposedMatrix
	{
		FTransform Transform;
		FVector MirrorSign = FVector::OneVector;
	};

	FDecomposedMatrix DecomposeMatrixForEditableTransform(const FMatrix& Matrix)
	{
		constexpr float Epsilon = 1.0e-6f;

		FDecomposedMatrix Result;
		Result.MirrorSign = MirrorSignFromMatrix(Matrix);

		const FVector PositiveScale = Matrix.GetScale();
		const FVector SignedScale = PositiveScale * Result.MirrorSign;

		FMatrix RotationMatrix = FMatrix::Identity;
		const float ScaleValues[3] = { SignedScale.X, SignedScale.Y, SignedScale.Z };
		for (int32 Row = 0; Row < 3; ++Row)
		{
			const float RowScale = ScaleValues[Row];
			if (std::fabs(RowScale) > Epsilon)
			{
				RotationMatrix.M[Row][0] = Matrix.M[Row][0] / RowScale;
				RotationMatrix.M[Row][1] = Matrix.M[Row][1] / RowScale;
				RotationMatrix.M[Row][2] = Matrix.M[Row][2] / RowScale;
			}
		}

		FQuat Rotation = RotationMatrix.ToQuat();
		Rotation.Normalize();
		Result.Transform = FTransform(Matrix.GetLocation(), Rotation, PositiveScale);
		return Result;
	}

	FTransform TransformFromMatrix(const FMatrix& Matrix)
	{
		return DecomposeMatrixForEditableTransform(Matrix).Transform;
	}

	FMatrix MakeTransformMatrixWithHiddenMirror(const FTransform& Transform, const FVector& MirrorSign)
	{
		return FTransform(Transform.Location, Transform.Rotation.GetNormalized(), AbsVector(Transform.Scale) * MirrorSign).ToMatrix();
	}

	void FoldNegativeScaleIntoMirror(FTransform& Transform, FVector& MirrorSign)
	{
		if (Transform.Scale.X < 0.0f) MirrorSign.X = -1.0f;
		if (Transform.Scale.Y < 0.0f) MirrorSign.Y = -1.0f;
		if (Transform.Scale.Z < 0.0f) MirrorSign.Z = -1.0f;
		Transform.Scale = AbsVector(Transform.Scale);
	}

	FVector SafeGetMirrorSign(const TArray<FVector>& MirrorSigns, int32 BoneIndex)
	{
		return BoneIndex >= 0 && BoneIndex < static_cast<int32>(MirrorSigns.size()) ? MirrorSigns[BoneIndex] : FVector::OneVector;
	}

	void EnsureMirrorSignsFromSkeleton(const FSkeleton& Skeleton, TArray<FVector>& MirrorSigns)
	{
		const int32 BoneCount = static_cast<int32>(Skeleton.Bones.size());
		if (static_cast<int32>(MirrorSigns.size()) == BoneCount)
		{
			return;
		}

		MirrorSigns.clear();
		MirrorSigns.reserve(BoneCount);
		for (const FBoneInfo& Bone : Skeleton.Bones)
		{
			MirrorSigns.push_back(DecomposeMatrixForEditableTransform(Bone.LocalBindPose).MirrorSign);
		}
	}

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::max)(MinValue, (std::min)(Value, MaxValue));
	}

	bool MakeBoneArmatureFrame(const FVector& Start, const FVector& End, FBoneArmatureFrame& OutFrame)
	{
		const FVector Delta = End - Start;
		const float Length = Delta.Length();
		if (Length <= BoneArmatureMinLength)
		{
			return false;
		}

		OutFrame.Start = Start;
		OutFrame.End = End;
		OutFrame.Axis = Delta / Length;
		OutFrame.Length = Length;
		OutFrame.Radius = ClampFloat(Length * BoneArmatureRadiusRatio, BoneArmatureMinRadius, BoneArmatureMaxRadius);

		const FVector UpHint = std::abs(OutFrame.Axis.Z) < 0.92f ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
		OutFrame.Right = UpHint.Cross(OutFrame.Axis).Normalized();
		OutFrame.Up = OutFrame.Axis.Cross(OutFrame.Right).Normalized();
		return true;
	}

	FVector GetArmatureBaseCenter(const FBoneArmatureFrame& Frame)
	{
		return Frame.Start + Frame.Axis * (Frame.Length * BoneArmatureBaseBias);
	}

	FVector4 ShadeArmatureColor(const FColor& BaseColor, const FVector& FaceNormal)
	{
		const FVector LightDir = FVector(-0.35f, 0.45f, 0.82f).Normalized();
		const FVector Normal = FaceNormal.Normalized();
		const float NdotL = std::abs(Normal.Dot(LightDir));
		const float Shade = 0.50f + NdotL * 0.50f;
		return FVector4(
			(BaseColor.R / 255.0f) * Shade,
			(BaseColor.G / 255.0f) * Shade,
			(BaseColor.B / 255.0f) * Shade,
			BaseColor.A / 255.0f
		);
	}

	void AddSolidArmatureTriangle(FScene& Scene, const FVector& V0, const FVector& V1, const FVector& V2, const FColor& BaseColor)
	{
		const FVector Normal = (V1 - V0).Cross(V2 - V0);
		Scene.AddForegroundDebugSolidTriangle(V0, V1, V2, ShadeArmatureColor(BaseColor, Normal));
	}

	void AddSolidArmatureQuad(FScene& Scene, const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3, const FColor& BaseColor)
	{
		AddSolidArmatureTriangle(Scene, V0, V1, V2, BaseColor);
		AddSolidArmatureTriangle(Scene, V0, V2, V3, BaseColor);
	}

	float GetArmatureJointRadius(const FBoneArmatureFrame& Frame)
	{
		return ClampFloat(Frame.Length * BoneJointRadiusRatio, BoneArmatureMinRadius, Frame.Radius * 0.65f);
	}

	void BuildArmatureJointTriangles(const FBoneArmatureFrame& Frame, const FVector& Center, TArray<FVector>& OutVertices)
	{
		const float Radius = GetArmatureJointRadius(Frame);
		constexpr int32 LatSegments = 4;
		constexpr int32 LonSegments = 8;
		FVector Points[LatSegments + 1][LonSegments];

		for (int32 Lat = 0; Lat <= LatSegments; ++Lat)
		{
			const float V = static_cast<float>(Lat) / static_cast<float>(LatSegments);
			const float Phi = -1.57079632679f + V * 3.14159265359f;
			const float AxisOffset = std::sin(Phi);
			const float RingScale = std::cos(Phi);

			for (int32 Lon = 0; Lon < LonSegments; ++Lon)
			{
				const float U = static_cast<float>(Lon) / static_cast<float>(LonSegments);
				const float Theta = U * 6.28318530718f;
				const FVector RingDir = Frame.Right * std::cos(Theta) + Frame.Up * std::sin(Theta);
				Points[Lat][Lon] = Center + (Frame.Axis * AxisOffset + RingDir * RingScale) * Radius;
			}
		}

		for (int32 Lat = 0; Lat < LatSegments; ++Lat)
		{
			for (int32 Lon = 0; Lon < LonSegments; ++Lon)
			{
				const int32 NextLon = (Lon + 1) % LonSegments;
				OutVertices.push_back(Points[Lat][Lon]);
				OutVertices.push_back(Points[Lat][NextLon]);
				OutVertices.push_back(Points[Lat + 1][NextLon]);
				OutVertices.push_back(Points[Lat][Lon]);
				OutVertices.push_back(Points[Lat + 1][NextLon]);
				OutVertices.push_back(Points[Lat + 1][Lon]);
			}
		}
	}

	void AddArmatureJoint(FScene& Scene, const FBoneArmatureFrame& Frame, const FVector& Center, const FColor& Color)
	{
		TArray<FVector> Triangles;
		Triangles.reserve(4 * 8 * 6);
		BuildArmatureJointTriangles(Frame, Center, Triangles);

		for (int32 Index = 0; Index + 2 < static_cast<int32>(Triangles.size()); Index += 3)
		{
			AddSolidArmatureTriangle(Scene, Triangles[Index], Triangles[Index + 1], Triangles[Index + 2], Color);
		}
	}

	void AddUnrealStyleArmatureBody(FScene& Scene, const FBoneArmatureFrame& Frame, const FColor& BodyColor)
	{
		const FVector WidePoint = GetArmatureBaseCenter(Frame);
		const float Pull = Frame.Length * 0.006f;
		const FVector Equator[4] =
		{
			WidePoint + Frame.Axis * Pull + Frame.Right * Frame.Radius,
			WidePoint - Frame.Axis * Pull + Frame.Up * Frame.Radius,
			WidePoint + Frame.Axis * Pull - Frame.Right * Frame.Radius,
			WidePoint - Frame.Axis * Pull - Frame.Up * Frame.Radius
		};

		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector& Current = Equator[Index];
			const FVector& Next = Equator[(Index + 1) % 4];
			AddSolidArmatureTriangle(Scene, Frame.Start, Current, Next, BodyColor);
			AddSolidArmatureTriangle(Scene, Frame.End, Next, Current, BodyColor);
		}
	}

	void AddArmatureBodyTriangles(const FBoneArmatureFrame& Frame, TArray<FVector>& OutVertices)
	{
		const FVector WidePoint = GetArmatureBaseCenter(Frame);
		const float Pull = Frame.Length * 0.006f;
		const FVector Equator[4] =
		{
			WidePoint + Frame.Axis * Pull + Frame.Right * Frame.Radius,
			WidePoint - Frame.Axis * Pull + Frame.Up * Frame.Radius,
			WidePoint + Frame.Axis * Pull - Frame.Right * Frame.Radius,
			WidePoint - Frame.Axis * Pull - Frame.Up * Frame.Radius
		};

		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector& Current = Equator[Index];
			const FVector& Next = Equator[(Index + 1) % 4];
			OutVertices.push_back(Frame.Start);
			OutVertices.push_back(Current);
			OutVertices.push_back(Next);
			OutVertices.push_back(Frame.End);
			OutVertices.push_back(Next);
			OutVertices.push_back(Current);
		}
	}

	bool RaycastArmatureTriangles(const FRay& Ray, const TArray<FVector>& Triangles, float& OutDistance)
	{
		bool bHit = false;
		float Closest = FLT_MAX;
		for (int32 Index = 0; Index + 2 < static_cast<int32>(Triangles.size()); Index += 3)
		{
			float T = 0.0f;
			if (FRayUtils::IntersectTriangle(Ray.Origin, Ray.Direction, Triangles[Index], Triangles[Index + 1], Triangles[Index + 2], T) && T < Closest)
			{
				Closest = T;
				bHit = true;
			}
		}

		OutDistance = Closest;
		return bHit;
	}

	bool RaycastArmatureFrame(const FRay& Ray, const FBoneArmatureFrame& Frame, float& OutDistance)
	{
		TArray<FVector> Triangles;
		Triangles.reserve(24);
		AddArmatureBodyTriangles(Frame, Triangles);
		return RaycastArmatureTriangles(Ray, Triangles, OutDistance);
	}

	bool RaycastArmatureJoint(const FRay& Ray, const FBoneArmatureFrame& Frame, const FVector& Center, float& OutDistance)
	{
		TArray<FVector> Triangles;
		Triangles.reserve(4 * 8 * 6);
		BuildArmatureJointTriangles(Frame, Center, Triangles);
		return RaycastArmatureTriangles(Ray, Triangles, OutDistance);
	}

	void AddSimpleSolidBoneVisual(FScene& Scene, const FVector& Start, const FVector& End, bool bSelected)
	{
		FBoneArmatureFrame Frame;
		if (!MakeBoneArmatureFrame(Start, End, Frame))
		{
			return;
		}

		const FColor BodyColor = bSelected ? FColor(255, 205, 64, 235) : FColor(90, 165, 235, 195);
		const FColor JointColor = bSelected ? FColor(255, 238, 130, 245) : FColor(178, 220, 255, 210);

		AddUnrealStyleArmatureBody(Scene, Frame, BodyColor);
		AddArmatureJoint(Scene, Frame, Start, JointColor);
		AddArmatureJoint(Scene, Frame, End, JointColor);
	}

}

USkeletalMeshComponent::~USkeletalMeshComponent() = default;

FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
	FMeshDataView SkinnedView = Super::GetMeshDataView();
	if (SkinnedView.IsValid())
	{
		return SkinnedView;
	}

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshLOD* LOD = MeshAsset ? MeshAsset->GetLOD(0) : nullptr;
	if (!LOD || LOD->Vertices.empty()) return {};

	FMeshDataView View;
	View.VertexData = LOD->Vertices.data();
	View.VertexCount = static_cast<uint32>(LOD->Vertices.size());
	View.Stride = sizeof(FSkeletalVertex);
	View.IndexData = LOD->Indices.data();
	View.IndexCount = static_cast<uint32>(LOD->Indices.size());
	return View;
}

void USkeletalMeshComponent::ContributeVisuals(FScene& Scene) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!ShouldDisplayBones() || !MeshAsset || MeshAsset->Skeleton.Bones.empty()) return;

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	const FMatrix& ComponentWorld = GetWorldMatrix();

	auto GetBoneMatrix = [&](int32 BoneIndex) -> FMatrix
	{
		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ComponentSpaceMatrices.size()))
			return ComponentSpaceMatrices[BoneIndex];
		return Skeleton.Bones[BoneIndex].GlobalBindPose;
	};

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
	{
		const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
		if (!IsValidBoneIndex(ParentIndex)) continue;

		const FVector ParentWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(ParentIndex).GetLocation());
		const FVector BoneWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(BoneIndex).GetLocation());
		const bool bSelected = BoneIndex == SelectedBoneIndex || ParentIndex == SelectedBoneIndex;
		AddSimpleSolidBoneVisual(Scene, ParentWorld, BoneWorld, bSelected);
	}
}

const FTransform* USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	return IsValidBoneIndex(BoneIndex) ? &BoneSpaceTransforms[BoneIndex] : nullptr;
}

const FTransform* USkeletalMeshComponent::GetBoneComponentSpaceTransform(int32 BoneIndex) const
{
	return IsValidBoneIndex(BoneIndex) ? &ComponentSpaceTransforms[BoneIndex] : nullptr;
}

bool USkeletalMeshComponent::GetBoneComponentSpaceMatrix(int32 BoneIndex, FMatrix& OutMatrix) const
{
	if (!IsValidBoneIndex(BoneIndex))
	{
		return false;
	}

	if (BoneIndex < static_cast<int32>(ComponentSpaceMatrices.size()))
	{
		OutMatrix = ComponentSpaceMatrices[BoneIndex];
		return true;
	}

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset || BoneIndex >= static_cast<int32>(MeshAsset->Skeleton.Bones.size()))
	{
		return false;
	}

	OutMatrix = MeshAsset->Skeleton.Bones[BoneIndex].GlobalBindPose;
	return true;
}

void USkeletalMeshComponent::SetSelectedBoneIndex(int32 BoneIndex)
{
	SelectedBoneIndex = IsValidBoneIndex(BoneIndex) ? BoneIndex : -1;
}

const FSkeletalSocket* USkeletalMeshComponent::FindSocketByName(const FString& SocketName) const
{
	const USkeletalMesh* Mesh      = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

	if (!MeshAsset)
	{
		return nullptr;
	}

	for (const FSkeletalSocket& Socket : MeshAsset->Sockets)
	{
		if (Socket.Name == SocketName)
		{
			return &Socket;
		}
	}

	return nullptr;
}

bool USkeletalMeshComponent::GetSocketComponentSpaceMatrix(const FString& SocketName, FMatrix& OutMatrix) const
{
	const USkeletalMesh* Mesh      = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

	if (!MeshAsset)
	{
		return false;
	}

	const FSkeletalSocket* Socket = FindSocketByName(SocketName);
	if (!Socket)
	{
		return false;
	}

	const int32 ParentBoneIndex = Socket->ParentBoneIndex;

	if (ParentBoneIndex < 0 || ParentBoneIndex >= static_cast<int32>(MeshAsset->Skeleton.Bones.size()))
	{
		return false;
	}

	FMatrix ParentBoneComponentMatrix = MeshAsset->Skeleton.Bones[ParentBoneIndex].GlobalBindPose;
	if (ParentBoneIndex < static_cast<int32>(ComponentSpaceMatrices.size()))
	{
		ParentBoneComponentMatrix = ComponentSpaceMatrices[ParentBoneIndex];
	}

	OutMatrix = Socket->LocalMatrixToParentBone * ParentBoneComponentMatrix;
	return true;
}

bool USkeletalMeshComponent::GetSocketWorldMatrix(const FString& SocketName, FMatrix& OutMatrix) const
{
	FMatrix SocketComponentMatrix;

	if (!GetSocketComponentSpaceMatrix(SocketName, SocketComponentMatrix))
	{
		return false;
	}

	OutMatrix = SocketComponentMatrix * GetWorldMatrix();
	return true;
}

int32 USkeletalMeshComponent::PickBoneArmature(const FRay& Ray, float* OutDistance) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset || MeshAsset->Skeleton.Bones.empty())
	{
		if (OutDistance) *OutDistance = FLT_MAX;
		return -1;
	}

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	const FMatrix& ComponentWorld = GetWorldMatrix();

	float BestDistance = FLT_MAX;
	int32 BestBoneIndex = -1;

	auto GetBoneMatrix = [&](int32 BoneIndex) -> FMatrix
	{
		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ComponentSpaceMatrices.size()))
		{
			return ComponentSpaceMatrices[BoneIndex];
		}
		return Skeleton.Bones[BoneIndex].GlobalBindPose;
	};

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
	{
		const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
		if (!IsValidBoneIndex(ParentIndex))
		{
			continue;
		}

		const FVector ParentWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(ParentIndex).GetLocation());
		const FVector BoneWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(BoneIndex).GetLocation());

		FBoneArmatureFrame Frame;
		if (!MakeBoneArmatureFrame(ParentWorld, BoneWorld, Frame))
		{
			continue;
		}

		float HitDistance = FLT_MAX;
		if (RaycastArmatureFrame(Ray, Frame, HitDistance) && HitDistance < BestDistance)
		{
			BestDistance = HitDistance;
			BestBoneIndex = BoneIndex;
		}

		if (RaycastArmatureJoint(Ray, Frame, ParentWorld, HitDistance) && HitDistance < BestDistance)
		{
			BestDistance = HitDistance;
			BestBoneIndex = ParentIndex;
		}
		if (RaycastArmatureJoint(Ray, Frame, BoneWorld, HitDistance) && HitDistance < BestDistance)
		{
			BestDistance = HitDistance;
			BestBoneIndex = BoneIndex;
		}
	}

	if (OutDistance)
	{
		*OutDistance = BestDistance;
	}
	return BestBoneIndex;
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	if (!IsValidBoneIndex(BoneIndex)) return;

	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size())
		InitializeBoneTransformsFromSkeleton();

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (MeshAsset)
	{
		EnsureMirrorSignsFromSkeleton(MeshAsset->Skeleton, BoneSpaceMirrorSigns);
	}

	FTransform NormalizedTransform = NewTransform;
	NormalizedTransform.Rotation.Normalize();
	FVector MirrorSign = SafeGetMirrorSign(BoneSpaceMirrorSigns, BoneIndex);
	FoldNegativeScaleIntoMirror(NormalizedTransform, MirrorSign);
	BoneSpaceTransforms[BoneIndex] = NormalizedTransform;
	if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(BoneSpaceMirrorSigns.size()))
	{
		BoneSpaceMirrorSigns[BoneIndex] = MirrorSign;
	}
	
	MarkSkeletalPoseDirty();
	RefreshBoneTransforms();
	UpdateSkinnedMeshObject();
}

bool USkeletalMeshComponent::SetBoneComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	if (!IsValidBoneIndex(BoneIndex)) return false;
	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size()) InitializeBoneTransformsFromSkeleton();

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset) return false;

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	EnsureMirrorSignsFromSkeleton(Skeleton, BoneSpaceMirrorSigns);

	FTransform EditableComponent = NewTransform;
	EditableComponent.Rotation.Normalize();
	EditableComponent.Scale = AbsVector(EditableComponent.Scale);

	FMatrix CurrentComponentMatrix = FMatrix::Identity;
	GetBoneComponentSpaceMatrix(BoneIndex, CurrentComponentMatrix);
	const FVector ComponentMirrorSign = MirrorSignFromMatrix(CurrentComponentMatrix);
	const FMatrix RequestedComponentMatrix = MakeTransformMatrixWithHiddenMirror(EditableComponent, ComponentMirrorSign);

	FTransform LocalTransform = BoneSpaceTransforms[BoneIndex];
	const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
	if (IsValidBoneIndex(ParentIndex))
	{
		const FMatrix ParentComponentMatrix = ParentIndex < static_cast<int32>(ComponentSpaceMatrices.size())
			? ComponentSpaceMatrices[ParentIndex]
			: Skeleton.Bones[ParentIndex].GlobalBindPose;

		const FMatrix RequestedLocalMatrix = RequestedComponentMatrix * ParentComponentMatrix.GetInverse();
		const FDecomposedMatrix RequestedLocal = DecomposeMatrixForEditableTransform(RequestedLocalMatrix);

		LocalTransform.Location = RequestedLocal.Transform.Location;
		LocalTransform.Rotation = RequestedLocal.Transform.Rotation.GetNormalized();
		LocalTransform.Scale = RequestedLocal.Transform.Scale;
		BoneSpaceMirrorSigns[BoneIndex] = RequestedLocal.MirrorSign;
	}
	else
	{
		const FDecomposedMatrix RequestedLocal = DecomposeMatrixForEditableTransform(RequestedComponentMatrix);
		LocalTransform = RequestedLocal.Transform;
		BoneSpaceMirrorSigns[BoneIndex] = RequestedLocal.MirrorSign;
	}

	SetBoneLocalTransform(BoneIndex, LocalTransform);
	return true;
}

bool USkeletalMeshComponent::SetBoneComponentSpaceLocation(int32 BoneIndex, const FVector& NewComponentLocation)
{
	if (!IsValidBoneIndex(BoneIndex)) return false;
	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size()) InitializeBoneTransformsFromSkeleton();

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset) return false;

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	FTransform LocalTransform = BoneSpaceTransforms[BoneIndex];
	const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
	if (IsValidBoneIndex(ParentIndex))
	{
		const FMatrix ParentComponentMatrix = ParentIndex < static_cast<int32>(ComponentSpaceMatrices.size())
			? ComponentSpaceMatrices[ParentIndex]
			: Skeleton.Bones[ParentIndex].GlobalBindPose;

		LocalTransform.Location = ParentComponentMatrix.GetInverse().TransformPositionWithW(NewComponentLocation);
	}
	else
	{
		LocalTransform.Location = NewComponentLocation;
	}

	SetBoneLocalTransform(BoneIndex, LocalTransform);
	return true;
}

bool USkeletalMeshComponent::SetBoneComponentSpaceRotation(int32 BoneIndex, const FQuat& NewComponentRotation)
{
	if (!IsValidBoneIndex(BoneIndex)) return false;
	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size()) InitializeBoneTransformsFromSkeleton();

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset) return false;

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	EnsureMirrorSignsFromSkeleton(Skeleton, BoneSpaceMirrorSigns);

	FQuat NewRotation = NewComponentRotation.GetNormalized();
	const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
	if (IsValidBoneIndex(ParentIndex))
	{
		const FMatrix ParentComponentMatrix = ParentIndex < static_cast<int32>(ComponentSpaceMatrices.size())
			? ComponentSpaceMatrices[ParentIndex]
			: Skeleton.Bones[ParentIndex].GlobalBindPose;

		FMatrix CurrentComponentMatrix = FMatrix::Identity;
		if (!GetBoneComponentSpaceMatrix(BoneIndex, CurrentComponentMatrix))
		{
			return false;
		}
		const FTransform CurrentComponentTransform = BoneIndex < static_cast<int32>(ComponentSpaceTransforms.size())
			? ComponentSpaceTransforms[BoneIndex]
			: TransformFromMatrix(CurrentComponentMatrix);
		const FVector ComponentMirrorSign = MirrorSignFromMatrix(CurrentComponentMatrix);

		const FMatrix RequestedComponentMatrix = MakeTransformMatrixWithHiddenMirror(
			FTransform(CurrentComponentTransform.Location, NewRotation, CurrentComponentTransform.Scale),
			ComponentMirrorSign);
		const FMatrix RequestedLocalMatrix = RequestedComponentMatrix * ParentComponentMatrix.GetInverse();

		NewRotation = DecomposeMatrixForEditableTransform(RequestedLocalMatrix).Transform.Rotation.GetNormalized();
	}

	FTransform LocalTransform = BoneSpaceTransforms[BoneIndex];
	LocalTransform.Rotation = NewRotation;
	SetBoneLocalTransform(BoneIndex, LocalTransform);
	return true;
}

void USkeletalMeshComponent::RefreshBoneTransforms()
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	const int32 BoneCount = Skeleton ? static_cast<int32>(Skeleton->Bones.size()) : 0;
	
	if (BoneCount == 0) return;
	if (BoneSpaceTransforms.size() != BoneCount) { InitializeBoneTransformsFromSkeleton(); return; }

	ComponentSpaceTransforms.resize(BoneCount);
	ComponentSpaceMatrices.assign(BoneCount, FMatrix::Identity);
	SkinningMatrices.assign(BoneCount, FMatrix::Identity);
	RequiredBones.resize(BoneCount);
	EnsureMirrorSignsFromSkeleton(*Skeleton, BoneSpaceMirrorSigns);

	FVector MinBound(1e10f, 1e10f, 1e10f), MaxBound(-1e10f, -1e10f, -1e10f);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		RequiredBones[i] = i;
		const int32 Parent = Skeleton->Bones[i].ParentIndex;
		FTransform& Local = BoneSpaceTransforms[i];
		Local.Rotation.Normalize();
		FoldNegativeScaleIntoMirror(Local, BoneSpaceMirrorSigns[i]);
		const FMatrix LocalMatrix = MakeTransformMatrixWithHiddenMirror(Local, BoneSpaceMirrorSigns[i]);

		if (Parent >= 0 && Parent < i)
		{
			ComponentSpaceMatrices[i] = LocalMatrix * ComponentSpaceMatrices[Parent];
		}
		else
		{
			ComponentSpaceMatrices[i] = LocalMatrix;
		}

		ComponentSpaceTransforms[i] = TransformFromMatrix(ComponentSpaceMatrices[i]);
		SkinningMatrices[i] = Skeleton->Bones[i].InverseBindPose * ComponentSpaceMatrices[i];

		// Dynamic Bounds Calculation
		const FVector Loc = ComponentSpaceMatrices[i].GetLocation();
		MinBound.X = std::min(MinBound.X, Loc.X); MinBound.Y = std::min(MinBound.Y, Loc.Y); MinBound.Z = std::min(MinBound.Z, Loc.Z);
		MaxBound.X = std::max(MaxBound.X, Loc.X); MaxBound.Y = std::max(MaxBound.Y, Loc.Y); MaxBound.Z = std::max(MaxBound.Z, Loc.Z);
	}

	// Apply Bounds Scale and Padding
	CachedLocalCenter = (MinBound + MaxBound) * 0.5f;
	CachedLocalExtent = (MaxBound - MinBound) * 0.5f * std::max(BoundsScale, 0.1f);
	CachedLocalExtent.X = std::max(CachedLocalExtent.X, 2.0f);
	CachedLocalExtent.Y = std::max(CachedLocalExtent.Y, 2.0f);
	CachedLocalExtent.Z = std::max(CachedLocalExtent.Z, 2.0f);
	bHasValidBounds = true;

	bRequiredBonesUpdated = true;
	bPoseDirty = false;
	bSkinningDirty = true;
	bBoundsDirty = true;
	MarkWorldBoundsDirty();
}

bool USkeletalMeshComponent::CaptureLocalPose(TArray<FSkeletalPoseDesc> &OutBones) const
{
	OutBones.clear();

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	if (!Skeleton || Skeleton->Bones.empty())
	{
		return false;
	}

	const int32 BoneCount = static_cast<int32>(Skeleton->Bones.size());
	if (BoneSpaceTransforms.size() != BoneCount)
	{
		return false;
	}

	OutBones.reserve(BoneCount);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const FBoneInfo& Bone = Skeleton->Bones[BoneIndex];
		FSkeletalPoseDesc Desc;
		Desc.BoneName = Bone.Name;
		Desc.ParentIndex = Bone.ParentIndex;
		Desc.LocalTransform = BoneSpaceTransforms[BoneIndex];
		OutBones.push_back(Desc);
	}

	return true;
}

bool USkeletalMeshComponent::ApplyLocalPose(const TArray<FSkeletalPoseDesc>& Bones)
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	if (!Skeleton || Skeleton->Bones.empty() || Bones.empty())
	{
		return false;
	}

	const int32 BoneCount = static_cast<int32>(Skeleton->Bones.size());
	if (BoneSpaceTransforms.size() != BoneCount)
	{
		InitializeBoneTransformsFromSkeleton();
	}
	if (BoneSpaceTransforms.size() != BoneCount)
	{
		return false;
	}

	bool bAppliedAny = false;
	for (int32 PoseIndex = 0; PoseIndex < static_cast<int32>(Bones.size()); ++PoseIndex)
	{
		const FSkeletalPoseDesc& Desc = Bones[PoseIndex];
		int32 TargetBoneIndex = -1;

		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			if (Skeleton->Bones[BoneIndex].Name == Desc.BoneName)
			{
				TargetBoneIndex = BoneIndex;
				break;
			}
		}

		if (TargetBoneIndex < 0 && PoseIndex < BoneCount && Skeleton->Bones[PoseIndex].ParentIndex == Desc.ParentIndex)
		{
			TargetBoneIndex = PoseIndex;
		}

		if (!IsValidBoneIndex(TargetBoneIndex))
		{
			continue;
		}

		EnsureMirrorSignsFromSkeleton(*Skeleton, BoneSpaceMirrorSigns);
		FTransform NormalizedTransform = Desc.LocalTransform;
		NormalizedTransform.Rotation.Normalize();
		FVector MirrorSign = SafeGetMirrorSign(BoneSpaceMirrorSigns, TargetBoneIndex);
		FoldNegativeScaleIntoMirror(NormalizedTransform, MirrorSign);
		BoneSpaceTransforms[TargetBoneIndex] = NormalizedTransform;
		BoneSpaceMirrorSigns[TargetBoneIndex] = MirrorSign;
		bAppliedAny = true;
	}

	if (!bAppliedAny)
	{
		return false;
	}

	MarkSkeletalPoseDirty();
	RefreshBoneTransforms();
	UpdateSkinnedMeshObject();
	return true;
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	SetSkeletalMeshInternal(InSkeletalMesh, false, false);
	RequiredBones.clear();
	SelectedBoneIndex = -1;
	RootBoneTranslation = FVector::ZeroVector;
	bRequiredBonesUpdated = false;
	InitializeBoneTransformsFromSkeleton();
	FinalizeSkeletalMeshRenderState();
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	USkinnedMeshComponent::Serialize(Ar);
	Ar << BoneSpaceTransforms;
	Ar << bForceRefPose;
	Ar << bEnableSkeletonUpdate;
	Ar << RootBoneTranslation;
	Ar << SkeletalPosePath;
	Ar << bShowBoneNames;

	if (Ar.IsLoading())
	{
		if (BoneSpaceTransforms.empty()) InitializeBoneTransformsFromSkeleton();
		else RefreshBoneTransforms();
	}
}

void USkeletalMeshComponent::PostDuplicate()
{
	USkinnedMeshComponent::PostDuplicate();
	RefreshBoneTransforms();
	UpdateSkinnedMeshObject();
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();
	UMeshComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });
	OutProps.push_back({ "Skeletal Pose", EPropertyType::SkeletalPoseRef, &SkeletalPosePath });
	OutProps.push_back({ "CPU Skinning", EPropertyType::Bool, &bCPUSkinning });
	OutProps.push_back({ "Bounds Scale", EPropertyType::Float, &BoundsScale, 0.1f, 10.0f, 0.1f });
	OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });
	OutProps.push_back({ "Show Bone Names", EPropertyType::Bool, &bShowBoneNames });
	OutProps.push_back({ "Selected Bone Index", EPropertyType::Int, &SelectedBoneIndex, -1.0f, 100000.0f, 1.0f });

	for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
		OutProps.push_back({ "Element " + std::to_string(i), EPropertyType::MaterialSlot, &MaterialSlots[i] });
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		SetSkeletalMesh(FAssetManager::Get().LoadSkeletalMesh({ SkeletalMeshPath }));
	}
	else if (std::strcmp(PropertyName, "Skeletal Pose") == 0)
	{
		if (!IsNonePath(SkeletalPosePath))
		{
			FString Error;
			UAssetData* LoadedAsset = FAssetFileSerializer::LoadAssetFromFile(ResolveProjectPath(SkeletalPosePath), &Error);
			if (USkeletalPoseAssetData* PoseAsset = Cast<USkeletalPoseAssetData>(LoadedAsset))
			{
				ApplyLocalPose(PoseAsset->Bones);
			}
			if (LoadedAsset)
			{
				UObjectManager::Get().DestroyObject(LoadedAsset);
			}
		}
	}
	else if (std::strncmp(PropertyName, "Element ", 8) == 0)
	{
		const int32 idx = std::atoi(&PropertyName[8]);
		if (idx >= 0 && idx < (int32)MaterialSlots.size())
			SetMaterial(idx, IsNonePath(MaterialSlots[idx].Path) ? nullptr : FMaterialManager::Get().LoadMaterial(MaterialSlots[idx].Path));
	}
	else if (std::strcmp(PropertyName, "Bounds Scale") == 0 || std::strcmp(PropertyName, "Force Ref Pose") == 0)
		MarkSkeletalPoseDirty();
	else if (std::strcmp(PropertyName, "CPU Skinning") == 0)
	{
		if (bCPUSkinning)
		{
			MarkSkeletalPoseDirty();
			RefreshBoneTransforms();
			UpdateSkinnedMeshObject();
		}
		else
		{
			MarkProxyDirty(EDirtyFlag::Mesh);
			MarkWorldBoundsDirty();
		}
	}
	else if (std::strcmp(PropertyName, "Selected Bone Index") == 0 || std::strcmp(PropertyName, "Display Bones") == 0)
		MarkProxyDirty(EDirtyFlag::Mesh);
}

void USkeletalMeshComponent::MarkSkeletalPoseDirty()
{
	bRequiredBonesUpdated = false;
	bPoseDirty = true;
	bSkinningDirty = true;
	bBoundsDirty = true;
	MarkProxyDirty(EDirtyFlag::Mesh);
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::InitializeBoneTransformsFromSkeleton()
{
	BoneSpaceTransforms.clear();
	BoneSpaceMirrorSigns.clear();
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	
	if (!Skeleton || Skeleton->Bones.empty())
	{
		SelectedBoneIndex = -1;
		bRequiredBonesUpdated = true;
		bPoseDirty = bSkinningDirty = false;
		return;
	}

	for (const FBoneInfo& Bone : Skeleton->Bones)
	{
		const FDecomposedMatrix Decomposed = DecomposeMatrixForEditableTransform(Bone.LocalBindPose);
		BoneSpaceTransforms.push_back(Decomposed.Transform);
		BoneSpaceMirrorSigns.push_back(Decomposed.MirrorSign);
	}

	RefreshBoneTransforms();
	UpdateSkinnedMeshObject();
}

bool USkeletalMeshComponent::IsValidBoneIndex(int32 BoneIndex) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	return MeshAsset && BoneIndex >= 0 && BoneIndex < (int32)MeshAsset->Skeleton.Bones.size();
}
