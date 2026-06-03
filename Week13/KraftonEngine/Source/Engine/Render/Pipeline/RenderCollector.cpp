#include "RenderCollector.h"

#include "Component/ActorComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Collision/ConvexVolume.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include "Debug/DebugDrawQueue.h"
#include "Render/Types/LODContext.h"
#include "Math/MathUtils.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Scene/FScene.h"
#include "Physics/Assets/PhysicsAsset.h"
#include "Physics/Assets/PhysicsBodySetup.h"
#include "Physics/Assets/PhysicsConstraintSetup.h"
#include "Physics/Assets/PhysicsShapeSetup.h"
#include <cmath>

#include <Collision/Octree.h>
#include <Collision/SpatialPartition.h>

// ============================================================
// UpdateProxyLOD — LOD 갱신 공통 헬퍼 (Collector + Builder 공유)
// ============================================================
void UpdateProxyLOD(FPrimitiveSceneProxy* Proxy, const FLODUpdateContext& LODCtx)
{
	if (!LODCtx.bValid || !LODCtx.ShouldRefreshLOD(Proxy->GetProxyId(), Proxy->GetLastLODUpdateFrame()))
		return;

	const FVector& Pos = Proxy->GetCachedWorldPos();
	const float dx = LODCtx.CameraPos.X - Pos.X;
	const float dy = LODCtx.CameraPos.Y - Pos.Y;
	const float dz = LODCtx.CameraPos.Z - Pos.Z;
	Proxy->UpdateLOD(SelectLOD(Proxy->GetCurrentLOD(), dx * dx + dy * dy + dz * dz));
	Proxy->SetLastLODUpdateFrame(LODCtx.LODUpdateFrame);
}

void FRenderCollector::Collect(UWorld* World, const FFrameContext& Frame, FCollectOutput& Output)
{
	if (!World) return;

	FScene& Scene = World->GetScene();
	Scene.UpdateDirtyProxies();

	Output.FrustumVisibleProxies.clear();
	{
		SCOPE_STAT_CAT("FrustumCulling", "3_Collect");
		const uint32 ExpectedCount = Scene.GetProxyCount()
			+ static_cast<uint32>(Scene.GetNeverCullProxies().size());
		if (Output.FrustumVisibleProxies.capacity() < ExpectedCount)
		{
			Output.FrustumVisibleProxies.reserve(ExpectedCount);
		}

		for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
		{
			if (Proxy)
			{
				Output.FrustumVisibleProxies.push_back(Proxy);
			}
		}

		World->GetPartition().QueryFrustumAllProxies(Frame.FrustumVolume, Output.FrustumVisibleProxies);
	}

	FilterVisibleProxies(Frame, Scene, Output);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FScene& Scene)
{
	Scene.SetGrid(GridSpacing, GridHalfLineCount);
}

void FRenderCollector::CollectDebugDraw(const FFrameContext& Frame, FScene& Scene)
{
	if (!Frame.RenderOptions.ShowFlags.bDebugDraw) return;

	for (const FDebugDrawItem& Item : Scene.GetDebugDrawQueue().GetItems())
	{
		Scene.AddDebugLine(Item.Start, Item.End, Item.Color, Item.bAlwaysOnTop);
	}
}

namespace
{
	static constexpr float PhysicsAssetCapsuleAxisScale = 1.0f;

	FMatrix BuildPhysicsAssetBoneWorldMatrix(const USkeletalMeshComponent* MeshComponent, const TArray<FMatrix>& BoneGlobals, int32 BoneIndex)
	{
		if (!MeshComponent)
		{
			return FMatrix::Identity;
		}

		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(BoneGlobals.size()))
		{
			return BoneGlobals[BoneIndex] * MeshComponent->GetWorldMatrix();
		}
		return MeshComponent->GetWorldMatrix();
	}

	int32 FindPhysicsAssetBoneIndex(const USkeletalMeshComponent* MeshComponent, const FName& BoneName)
	{
		if (!MeshComponent || BoneName == FName::None)
		{
			return -1;
		}

		const FString TargetName = BoneName.ToString();
		int32 BoneIndex = MeshComponent->FindBoneIndexByName(TargetName);
		if (BoneIndex >= 0)
		{
			return BoneIndex;
		}

		USkeletalMesh* Mesh = MeshComponent->GetSkeletalMesh();
		const FSkeletonAsset* SkeletonAsset = Mesh ? Mesh->GetSkeletonAsset() : nullptr;
		if (!SkeletonAsset)
		{
			return -1;
		}

		for (int32 Index = 0; Index < static_cast<int32>(SkeletonAsset->Bones.size()); ++Index)
		{
			if (SkeletonAsset->Bones[Index].Name == TargetName)
			{
				return Index;
			}
		}
		return -1;
	}

	FVector TransformPhysicsAssetPoint(const FMatrix& Matrix, const FVector& Point)
	{
		return Matrix.TransformPositionWithW(Point);
	}

	FVector TransformPhysicsAssetVector(const FMatrix& Matrix, const FVector& Vector)
	{
		return Matrix.TransformVector(Vector);
	}

	float SafeVectorLength(const FVector& Vector, float Fallback = 1.0f)
	{
		const float Len = Vector.Length();
		return Len > 0.0001f ? Len : Fallback;
	}

	FVector SafeNormalized(const FVector& Vector, const FVector& Fallback)
	{
		const float Len = Vector.Length();
		return Len > 0.0001f ? (Vector / Len) : Fallback;
	}

	void AddDebugCircle(FScene& Scene, const FVector& Center, const FVector& AxisA, const FVector& AxisB, float Radius, const FColor& Color)
	{
		constexpr int32 Segments = 24;
		FVector Prev = Center + AxisA * Radius;
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float T = (2.0f * FMath::Pi * static_cast<float>(i)) / static_cast<float>(Segments);
			const FVector P = Center + AxisA * (std::cos(T) * Radius) + AxisB * (std::sin(T) * Radius);
			Scene.AddDebugLine(Prev, P, Color, true);
			Prev = P;
		}
	}

	void AddDebugSphere(FScene& Scene, const FMatrix& WorldMatrix, float Radius, const FColor& Color)
	{
		const FVector Center = TransformPhysicsAssetPoint(WorldMatrix, FVector::ZeroVector);
		// Matrix basis length is intentionally preserved so component scale/non-uniform scale
		// affects the debug shape in the level editor. This is editor overlay data only;
		// no transient preview component is added to the level.
		FVector X = TransformPhysicsAssetVector(WorldMatrix, FVector::ForwardVector);
		FVector Y = TransformPhysicsAssetVector(WorldMatrix, FVector::RightVector);
		FVector Z = TransformPhysicsAssetVector(WorldMatrix, FVector::UpVector);
		AddDebugCircle(Scene, Center, X, Y, Radius, Color);
		AddDebugCircle(Scene, Center, X, Z, Radius, Color);
		AddDebugCircle(Scene, Center, Y, Z, Radius, Color);
		// Extra latitude rings make Body Shapes readable over a shaded skeletal mesh.
		AddDebugCircle(Scene, Center + Z * (Radius * 0.5f), X, Y, Radius * 0.866f, Color);
		AddDebugCircle(Scene, Center - Z * (Radius * 0.5f), X, Y, Radius * 0.866f, Color);
	}

	void AddDebugBox(FScene& Scene, const FMatrix& WorldMatrix, const FVector& HalfExtent, const FColor& Color)
	{
		const FVector Corners[8] =
		{
			FVector(-HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z), FVector( HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
			FVector( HalfExtent.X,  HalfExtent.Y, -HalfExtent.Z), FVector(-HalfExtent.X,  HalfExtent.Y, -HalfExtent.Z),
			FVector(-HalfExtent.X, -HalfExtent.Y,  HalfExtent.Z), FVector( HalfExtent.X, -HalfExtent.Y,  HalfExtent.Z),
			FVector( HalfExtent.X,  HalfExtent.Y,  HalfExtent.Z), FVector(-HalfExtent.X,  HalfExtent.Y,  HalfExtent.Z),
		};
		FVector W[8];
		for (int32 i = 0; i < 8; ++i)
		{
			W[i] = TransformPhysicsAssetPoint(WorldMatrix, Corners[i]);
		}
		const int32 Edges[12][2] =
		{
			{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
		};
		for (const auto& Edge : Edges)
		{
			Scene.AddDebugLine(W[Edge[0]], W[Edge[1]], Color, true);
		}
	}

	void AddDebugCapsule(FScene& Scene, const FMatrix& WorldMatrix, float Radius, float Length, const FColor& Color)
	{
		const FVector RawX = TransformPhysicsAssetVector(WorldMatrix, FVector::ForwardVector);
		const FVector RawY = TransformPhysicsAssetVector(WorldMatrix, FVector::RightVector);
		const FVector RawZ = TransformPhysicsAssetVector(WorldMatrix, FVector::UpVector);
		const float ScaleX = SafeVectorLength(RawX);
		const float ScaleY = SafeVectorLength(RawY);
		const float ScaleZ = SafeVectorLength(RawZ);
		const FVector AxisX = SafeNormalized(RawX, FVector::ForwardVector);
		const FVector AxisY = SafeNormalized(RawY, FVector::RightVector);
		const FVector AxisZ = SafeNormalized(RawZ, FVector::UpVector);

		const FVector Center = TransformPhysicsAssetPoint(WorldMatrix, FVector::ZeroVector);
		const float HalfLength = Length * ScaleX * 0.5f * PhysicsAssetCapsuleAxisScale;
		const float RadiusY = Radius * ScaleY;
		const float RadiusZ = Radius * ScaleZ;
		const float CapRadiusX = (RadiusY + RadiusZ) * 0.5f;
		const FVector A = Center + AxisX * HalfLength;
		const FVector B = Center - AxisX * HalfLength;

		AddDebugCircle(Scene, A, AxisY * ScaleY, AxisZ * ScaleZ, Radius, Color);
		AddDebugCircle(Scene, B, AxisY * ScaleY, AxisZ * ScaleZ, Radius, Color);

		Scene.AddDebugLine(A + AxisY * RadiusY, B + AxisY * RadiusY, Color, true);
		Scene.AddDebugLine(A - AxisY * RadiusY, B - AxisY * RadiusY, Color, true);
		Scene.AddDebugLine(A + AxisZ * RadiusZ, B + AxisZ * RadiusZ, Color, true);
		Scene.AddDebugLine(A - AxisZ * RadiusZ, B - AxisZ * RadiusZ, Color, true);

		constexpr int32 Segments = 12;
		for (int32 Plane = 0; Plane < 2; ++Plane)
		{
			const FVector Side = (Plane == 0) ? AxisY : AxisZ;
			const FVector Other = (Plane == 0) ? AxisZ : AxisY;
			const float SideRadius = (Plane == 0) ? RadiusY : RadiusZ;
			FVector PrevPos = A + Side * SideRadius;
			FVector PrevNeg = B + Side * SideRadius;
			for (int32 i = 1; i <= Segments; ++i)
			{
				const float T = (FMath::Pi * static_cast<float>(i)) / static_cast<float>(Segments);
				const FVector Pos = A + Side * (std::cos(T) * SideRadius) + AxisX * (std::sin(T) * CapRadiusX);
				const FVector Neg = B + Side * (std::cos(T) * SideRadius) - AxisX * (std::sin(T) * CapRadiusX);
				Scene.AddDebugLine(PrevPos, Pos, Color, true);
				Scene.AddDebugLine(PrevNeg, Neg, Color, true);
				PrevPos = Pos;
				PrevNeg = Neg;
			}
			(void)Other;
		}
	}



	void AddSolidTriangle(FScene& Scene, const FVector& A, const FVector& B, const FVector& C, const FVector4& Color)
	{
		FScene::FTranslucentDebugMesh& Mesh = Scene.AddTranslucentDebugMesh();
		Mesh.Vertices.push_back({ A, Color, 0 });
		Mesh.Vertices.push_back({ B, Color, 0 });
		Mesh.Vertices.push_back({ C, Color, 0 });
		Mesh.Indices = { 0, 1, 2 };
		Mesh.Center = (A + B + C) / 3.0f;
	}

	void AppendSolidVertex(FScene::FTranslucentDebugMesh& Mesh, const FMatrix& WorldMatrix, const FVector& LocalPosition, const FVector4& Color)
	{
		Mesh.Vertices.push_back({ TransformPhysicsAssetPoint(WorldMatrix, LocalPosition), Color, 0 });
	}

	void AddSolidBox(FScene& Scene, const FMatrix& WorldMatrix, const FVector& HalfExtent, const FVector4& Color)
	{
		FScene::FTranslucentDebugMesh& Mesh = Scene.AddTranslucentDebugMesh();
		const FVector Corners[8] =
		{
			FVector(-HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z), FVector( HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
			FVector( HalfExtent.X,  HalfExtent.Y, -HalfExtent.Z), FVector(-HalfExtent.X,  HalfExtent.Y, -HalfExtent.Z),
			FVector(-HalfExtent.X, -HalfExtent.Y,  HalfExtent.Z), FVector( HalfExtent.X, -HalfExtent.Y,  HalfExtent.Z),
			FVector( HalfExtent.X,  HalfExtent.Y,  HalfExtent.Z), FVector(-HalfExtent.X,  HalfExtent.Y,  HalfExtent.Z),
		};
		for (const FVector& Corner : Corners)
		{
			AppendSolidVertex(Mesh, WorldMatrix, Corner, Color);
		}
		Mesh.Indices =
		{
			0,2,1, 0,3,2,
			4,5,6, 4,6,7,
			0,1,5, 0,5,4,
			1,2,6, 1,6,5,
			2,3,7, 2,7,6,
			3,0,4, 3,4,7
		};
		Mesh.Center = TransformPhysicsAssetPoint(WorldMatrix, FVector::ZeroVector);
	}

	void AddSolidSphere(FScene& Scene, const FMatrix& WorldMatrix, float Radius, const FVector4& Color)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		constexpr int32 Slices = 20;
		constexpr int32 Stacks = 12;
		FScene::FTranslucentDebugMesh& Mesh = Scene.AddTranslucentDebugMesh();

		for (int32 i = 0; i <= Stacks; ++i)
		{
			const float Phi = FMath::Pi * static_cast<float>(i) / static_cast<float>(Stacks);
			const float SinPhi = std::sin(Phi);
			const float CosPhi = std::cos(Phi);
			for (int32 j = 0; j <= Slices; ++j)
			{
				const float Theta = 2.0f * FMath::Pi * static_cast<float>(j) / static_cast<float>(Slices);
				const FVector Local(
					Radius * SinPhi * std::cos(Theta),
					Radius * SinPhi * std::sin(Theta),
					Radius * CosPhi);
				AppendSolidVertex(Mesh, WorldMatrix, Local, Color);
			}
		}

		for (int32 i = 0; i < Stacks; ++i)
		{
			for (int32 j = 0; j < Slices; ++j)
			{
				const uint32 A = static_cast<uint32>(i * (Slices + 1) + j);
				const uint32 B = static_cast<uint32>((i + 1) * (Slices + 1) + j);
				const uint32 C = A + 1;
				const uint32 D = B + 1;
				Mesh.Indices.push_back(A); Mesh.Indices.push_back(B); Mesh.Indices.push_back(C);
				Mesh.Indices.push_back(C); Mesh.Indices.push_back(B); Mesh.Indices.push_back(D);
			}
		}
		Mesh.Center = TransformPhysicsAssetPoint(WorldMatrix, FVector::ZeroVector);
	}

	void AddSolidCapsule(FScene& Scene, const FMatrix& WorldMatrix, float Radius, float Length, const FVector4& Color)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		constexpr int32 Slices = 20;
		constexpr int32 CapSegments = 6;
		const float HalfLength = (std::max)(Length * 0.5f, 0.0f);
		FScene::FTranslucentDebugMesh& Mesh = Scene.AddTranslucentDebugMesh();

		auto AddRing = [&](float X, float R)
		{
			for (int32 j = 0; j <= Slices; ++j)
			{
				const float Theta = 2.0f * FMath::Pi * static_cast<float>(j) / static_cast<float>(Slices);
				AppendSolidVertex(Mesh, WorldMatrix, FVector(X, R * std::cos(Theta), R * std::sin(Theta)), Color);
			}
		};

		for (int32 i = 0; i <= CapSegments; ++i)
		{
			const float T = -FMath::Pi * 0.5f + (FMath::Pi * 0.5f) * static_cast<float>(i) / static_cast<float>(CapSegments);
			AddRing(-HalfLength + std::sin(T) * Radius, std::cos(T) * Radius);
		}
		AddRing(HalfLength, Radius);
		for (int32 i = 1; i <= CapSegments; ++i)
		{
			const float T = (FMath::Pi * 0.5f) * static_cast<float>(i) / static_cast<float>(CapSegments);
			AddRing(HalfLength + std::sin(T) * Radius, std::cos(T) * Radius);
		}

		const int32 RingCount = static_cast<int32>(Mesh.Vertices.size()) / (Slices + 1);
		for (int32 i = 0; i < RingCount - 1; ++i)
		{
			for (int32 j = 0; j < Slices; ++j)
			{
				const uint32 A = static_cast<uint32>(i * (Slices + 1) + j);
				const uint32 B = static_cast<uint32>((i + 1) * (Slices + 1) + j);
				const uint32 C = A + 1;
				const uint32 D = B + 1;
				Mesh.Indices.push_back(A); Mesh.Indices.push_back(B); Mesh.Indices.push_back(C);
				Mesh.Indices.push_back(C); Mesh.Indices.push_back(B); Mesh.Indices.push_back(D);
			}
		}
		Mesh.Center = TransformPhysicsAssetPoint(WorldMatrix, FVector::ZeroVector);
	}

	void AddDebugLowSphere(FScene& Scene, const FVector& Center, float Radius, const FColor& Color)
	{
		const FVector AxisX(1.0f, 0.0f, 0.0f);
		const FVector AxisY(0.0f, 1.0f, 0.0f);
		const FVector AxisZ(0.0f, 0.0f, 1.0f);
		AddDebugCircle(Scene, Center, AxisX, AxisY, Radius, Color);
		AddDebugCircle(Scene, Center, AxisY, AxisZ, Radius, Color);
		AddDebugCircle(Scene, Center, AxisZ, AxisX, Radius, Color);
	}

	void AddDebugBonePyramid(FScene& Scene, const FVector& Start, const FVector& End, float WidthScale, const FColor& Color)
	{
		FVector BoneVector = End - Start;
		const float Length = BoneVector.Length();
		if (Length <= 0.001f)
		{
			return;
		}

		const FVector Dir = BoneVector / Length;
		const FVector UpHint = std::fabs(Dir.Z) > 0.9f ? FVector(1.0f, 0.0f, 0.0f) : FVector(0.0f, 0.0f, 1.0f);
		const FVector Right = UpHint.Cross(Dir).Normalized();
		const FVector Up = Dir.Cross(Right).Normalized();
		const float HalfWidth = Length * WidthScale;

		const FVector C0 = End + Right * HalfWidth + Up * HalfWidth;
		const FVector C1 = End - Right * HalfWidth + Up * HalfWidth;
		const FVector C2 = End - Right * HalfWidth - Up * HalfWidth;
		const FVector C3 = End + Right * HalfWidth - Up * HalfWidth;

		Scene.AddDebugLine(Start, C0, Color, true);
		Scene.AddDebugLine(Start, C1, Color, true);
		Scene.AddDebugLine(Start, C2, Color, true);
		Scene.AddDebugLine(Start, C3, Color, true);
		Scene.AddDebugLine(C0, C1, Color, true);
		Scene.AddDebugLine(C1, C2, Color, true);
		Scene.AddDebugLine(C2, C3, Color, true);
		Scene.AddDebugLine(C3, C0, Color, true);
	}

	void AddConstraintAxisDebug(FScene& Scene, const FMatrix& FrameMatrix, const FVector& OtherOrigin, float AxisLen, const FColor& Color)
	{
		const FVector Origin = TransformPhysicsAssetPoint(FrameMatrix, FVector::ZeroVector);
		FVector X = SafeNormalized(TransformPhysicsAssetVector(FrameMatrix, FVector::ForwardVector), FVector::ForwardVector);
		FVector Y = SafeNormalized(TransformPhysicsAssetVector(FrameMatrix, FVector::RightVector), FVector::RightVector);
		FVector Z = SafeNormalized(TransformPhysicsAssetVector(FrameMatrix, FVector::UpVector), FVector::UpVector);
		Scene.AddDebugLine(Origin, Origin + X * AxisLen, FColor(70, 140, 255), true);
		Scene.AddDebugLine(Origin, Origin + Y * AxisLen, FColor(255, 90, 90), true);
		Scene.AddDebugLine(Origin, Origin + Z * AxisLen, FColor(90, 255, 90), true);
		Scene.AddDebugLine(Origin, OtherOrigin, Color, true);
	}
}


void FRenderCollector::CollectSkeletalMeshBoneDebug(UWorld* World, const FFrameContext& Frame, FScene& Scene)
{
	if (!World || Frame.WorldType != EWorldType::Editor || !Frame.RenderOptions.ShowFlags.bBones)
	{
		return;
	}

	const FColor BoneColor(125, 232, 122);
	constexpr float PyramidWidthScale = 0.03f;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Component);
			if (!MeshComponent || !MeshComponent->IsVisible())
			{
				continue;
			}

			USkeletalMesh* Mesh = MeshComponent->GetSkeletalMesh();
			FSkeletonAsset* SkeletonAsset = Mesh ? Mesh->GetSkeletonAsset() : nullptr;
			if (!SkeletonAsset)
			{
				continue;
			}

			const int32 BoneCount = static_cast<int32>(SkeletonAsset->Bones.size());
			if (BoneCount <= 0)
			{
				continue;
			}

			const FBoundingBox Bounds = MeshComponent->GetWorldBoundingBox();
			const float ModelSize = Bounds.GetExtent().Length();
			const float JointRadius = (std::max)(ModelSize * 0.01f, 0.005f);

			for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
			{
				const FVector BonePos = MeshComponent->GetBoneLocationByIndex(BoneIndex);
				AddDebugLowSphere(Scene, BonePos, JointRadius, BoneColor);

				const int32 ParentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
				if (ParentIndex >= 0 && ParentIndex < BoneCount)
				{
					const FVector ParentPos = MeshComponent->GetBoneLocationByIndex(ParentIndex);
					AddDebugBonePyramid(Scene, BonePos, ParentPos, PyramidWidthScale, BoneColor);
				}
			}
		}
	}
}

void FRenderCollector::CollectPhysicsAssetDebug(UWorld* World, const FFrameContext& Frame, FScene& Scene)
{
	if (!World || Frame.WorldType != EWorldType::Editor)
	{
		return;
	}

	const FShowFlags& Flags = Frame.RenderOptions.ShowFlags;
	const bool bDrawBodyShapes = Flags.bPhysicsBodyShapes;
	const bool bDrawConstraints = Flags.bPhysicsConstraints;
	if (!bDrawBodyShapes && !bDrawConstraints)
	{
		return;
	}

	const FColor ShapeColor(255, 255, 255, 90);
	const FColor ConstraintColor(255, 200, 40);

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Component);
			if (!MeshComponent || !MeshComponent->IsVisible())
			{
				continue;
			}

			UPhysicsAsset* PhysicsAsset = MeshComponent->GetPhysicsAsset();
			if (!PhysicsAsset)
			{
				continue;
			}

			TArray<FMatrix> BoneGlobals;
			MeshComponent->GetCurrentBoneGlobalMatrices(BoneGlobals);

			const FBoundingBox MeshBounds = MeshComponent->GetWorldBoundingBox();
			const float MeshExtentLen = (std::max)(MeshBounds.GetExtent().Length(), 1.0f);
			const float ConstraintAxisLen = (std::max)(MeshExtentLen * 0.08f, 2.0f);

			if (bDrawBodyShapes)
			{
				const FVector4 BodyColor = ShapeColor.ToVector4();
				for (const UPhysicsBodySetup* BodySetup : PhysicsAsset->GetBodySetups())
				{
					if (!BodySetup)
					{
						continue;
					}

					const int32 BoneIndex = FindPhysicsAssetBoneIndex(MeshComponent, BodySetup->GetTargetBoneName());
					const FMatrix BoneWorld = BuildPhysicsAssetBoneWorldMatrix(MeshComponent, BoneGlobals, BoneIndex);
					const FPhysicsAggregateShapeSetup& Shapes = BodySetup->GetShapeSetup();

					for (const FPhysicsSphereShapeSetup& Shape : Shapes.SphereShapeSetups)
					{
						const FMatrix ShapeWorld = Shape.LocalTransform.ToMatrix() * BoneWorld;
						AddSolidSphere(Scene, ShapeWorld, Shape.Radius, BodyColor);
						AddDebugSphere(Scene, ShapeWorld, Shape.Radius, FColor(255, 255, 255, 180));
					}
					for (const FPhysicsBoxShapeSetup& Shape : Shapes.BoxShapeSetups)
					{
						const FMatrix ShapeWorld = Shape.LocalTransform.ToMatrix() * BoneWorld;
						AddSolidBox(Scene, ShapeWorld, Shape.HalfExtent, BodyColor);
						AddDebugBox(Scene, ShapeWorld, Shape.HalfExtent, FColor(255, 255, 255, 180));
					}
					for (const FPhysicsCapsuleShapeSetup& Shape : Shapes.CapsuleShapeSetups)
					{
						const FMatrix ShapeWorld = Shape.LocalTransform.ToMatrix() * BoneWorld;
						AddSolidCapsule(Scene, ShapeWorld, Shape.Radius, Shape.Length, BodyColor);
						AddDebugCapsule(Scene, ShapeWorld, Shape.Radius, Shape.Length, FColor(255, 255, 255, 180));
					}
				}
			}

			if (bDrawConstraints)
			{
				for (const FPhysicsConstraintSetup& Constraint : PhysicsAsset->GetConstraintSetups())
				{
					const int32 ParentBone = FindPhysicsAssetBoneIndex(MeshComponent, Constraint.ParentBoneName);
					const int32 ChildBone = FindPhysicsAssetBoneIndex(MeshComponent, Constraint.ChildBoneName);
					if (ParentBone < 0 || ChildBone < 0)
					{
						continue;
					}

					const FMatrix ParentBoneWorld = BuildPhysicsAssetBoneWorldMatrix(MeshComponent, BoneGlobals, ParentBone);
					const FMatrix ChildBoneWorld = BuildPhysicsAssetBoneWorldMatrix(MeshComponent, BoneGlobals, ChildBone);
					const FMatrix ParentWorld = Constraint.ParentLocalFrame.ToMatrix() * ParentBoneWorld;
					const FMatrix ChildWorld = Constraint.ChildLocalFrame.ToMatrix() * ChildBoneWorld;
					FVector ParentOrigin = TransformPhysicsAssetPoint(ParentWorld, FVector::ZeroVector);
					FVector ChildOrigin = TransformPhysicsAssetPoint(ChildWorld, FVector::ZeroVector);
					// Some imported assets store tiny/identity local frames. Keep the constraint visible
					// in the level viewport by falling back to the body bone centers when the two
					// frame origins nearly overlap.
					if ((ChildOrigin - ParentOrigin).Length() < ConstraintAxisLen * 0.25f)
					{
						ParentOrigin = TransformPhysicsAssetPoint(ParentBoneWorld, FVector::ZeroVector);
						ChildOrigin = TransformPhysicsAssetPoint(ChildBoneWorld, FVector::ZeroVector);
					}
					const FVector MidOrigin = (ParentOrigin + ChildOrigin) * 0.5f;
					Scene.AddDebugLine(ParentOrigin, ChildOrigin, ConstraintColor, true);
					Scene.AddDebugLine(MidOrigin, ParentOrigin, FColor(255, 120, 80), true);
					Scene.AddDebugLine(MidOrigin, ChildOrigin, FColor(80, 255, 120), true);
					AddConstraintAxisDebug(Scene, ParentWorld, MidOrigin, ConstraintAxisLen, ConstraintColor);
					AddConstraintAxisDebug(Scene, ChildWorld, MidOrigin, ConstraintAxisLen, ConstraintColor);

					// Level viewport에서는 constraint frame이 mesh 내부에 묻히기 쉽다.
					// 그래서 부모/자식 bone 중점에도 크게 축을 한 번 더 그려서 토글 결과를 확실히 보이게 한다.
					FMatrix MidFrame = ParentWorld;
					MidFrame.M[3][0] = MidOrigin.X;
					MidFrame.M[3][1] = MidOrigin.Y;
					MidFrame.M[3][2] = MidOrigin.Z;
					AddConstraintAxisDebug(Scene, MidFrame, ChildOrigin, ConstraintAxisLen * 1.25f, ConstraintColor);
				}
			}
		}
	}
}

// ============================================================
// Octree 디버그 시각화 — 깊이별 색상으로 노드 AABB 표시
// ============================================================
static const FColor OctreeDepthColors[] = {
	FColor(255,   0,   0),	// 0: Red
	FColor(255, 165,   0),	// 1: Orange
	FColor(255, 255,   0),	// 2: Yellow
	FColor(0, 255,   0),	// 3: Green
	FColor(0, 255, 255),	// 4: Cyan
	FColor(0,   0, 255),	// 5: Blue
};

void FRenderCollector::CollectOctreeDebug(const FOctree* Node, FScene& Scene, uint32 Depth)
{
	if (!Node) return;

	const FBoundingBox& Bounds = Node->GetCellBounds();
	if (!Bounds.IsValid()) return;

	Scene.AddDebugAABB(Bounds.Min, Bounds.Max, OctreeDepthColors[Depth % 6]);

	for (const FOctree* Child : Node->GetChildren())
	{
		CollectOctreeDebug(Child, Scene, Depth + 1);
	}
}

// ============================================================
// FilterVisibleProxies — visibility/occlusion 필터 → RenderableProxies
// ============================================================
static bool ShouldCollectProxyForView(const FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame)
{
	if (!Proxy)
		return false;

	// Light View에서는 EditorOnly 프록시(빌보드 아이콘 등) 제외
	if (Frame.bIsLightView && Proxy->HasProxyFlag(EPrimitiveProxyFlags::EditorOnly))
		return false;

	if (!Frame.RenderOptions.ShowFlags.bStaticMesh &&
		Proxy->HasProxyFlag(EPrimitiveProxyFlags::StaticMesh))
		return false;

	if (!Frame.RenderOptions.ShowFlags.bSkeletalMesh &&
		Proxy->HasProxyFlag(EPrimitiveProxyFlags::SkeletalMesh))
		return false;

	return true;
}

void FRenderCollector::FilterVisibleProxies(const FFrameContext& Frame, FScene& Scene, FCollectOutput& Output)
{
	SCOPE_STAT_CAT("CollectVisibleProxy", "3_Collect");

	Output.VisibleProxySet.reserve(Output.FrustumVisibleProxies.size());
	for (FPrimitiveSceneProxy* Proxy : Output.FrustumVisibleProxies)
	{
		if (Proxy)
			Output.VisibleProxySet.insert(Proxy);
	}

	const FGPUOcclusionCulling* Occlusion = Frame.OcclusionCulling;
	FGPUOcclusionCulling* OcclusionMut = Frame.OcclusionCulling;

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->BeginGatherAABB(static_cast<uint32>(Output.FrustumVisibleProxies.size()));

	LOD_STATS_RESET();

	Output.RenderableProxies.reserve(Output.FrustumVisibleProxies.size());

	for (FPrimitiveSceneProxy* Proxy : Output.FrustumVisibleProxies)
	{
		
		if (!ShouldCollectProxyForView(Proxy, Frame))
		{
			continue;
		}

		UpdateProxyLOD(Proxy, Frame.LODContext);
		LOD_STATS_RECORD(Proxy->GetCurrentLOD());

		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::PerViewportUpdate))
		{
			Proxy->UpdatePerViewport(Frame);
		}

		if (!Proxy->IsVisible())
		{
			continue;
		}

		if (OcclusionMut)
		{
			OcclusionMut->GatherAABB(Proxy);
		}

		if (Occlusion && !Proxy->HasProxyFlag(EPrimitiveProxyFlags::NeverCull) && Occlusion->IsOccluded(Proxy))
		{
			continue;
		}

		Output.RenderableProxies.push_back(Proxy);
	}

	// 선택된 Actor의 컴포넌트 디버그 시각화 (빛 등 프록시 없는 Comp 포함)
	CollectSelectedActorVisuals(Scene);

	if (OcclusionMut && OcclusionMut->IsInitialized())
		OcclusionMut->EndGatherAABB();
}

// ============================================================
// CollectSelectedActorVisuals — Actor 단위 디버그 시각화 (빛 등 프록시 없는 Comp 포함)
// ============================================================
void FRenderCollector::CollectSelectedActorVisuals(FScene& Scene)
{
	for (AActor* Actor : Scene.GetSelectedActors())
	{
		if (!Actor) continue;
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp)
				Comp->ContributeSelectedVisuals(Scene);
		}
	}
}
