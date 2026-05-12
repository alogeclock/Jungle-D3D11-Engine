#include "Component/SkeletalMeshComponent.h"

#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cstdlib>
#include <cstring>
#include <string>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	// 에디터에서 비어 있는 에셋/머티리얼 경로를 None으로 취급한다.
	bool IsNonePath(const FString& Path)
	{
		return Path.empty() || Path == "None";
	}

	// FMatrix에서 위치, 회전, 스케일을 FTransform 형태로 복원한다.
	FTransform TransformFromMatrix(const FMatrix& Matrix)
	{
		FQuat Rotation = Matrix.ToQuat();
		Rotation.Normalize();
		return FTransform(Matrix.GetLocation(), Rotation, Matrix.GetScale());
	}
}

USkeletalMeshComponent::~USkeletalMeshComponent() = default;

// CPU 스키닝과 선택 보조 기능에서 사용할 LOD0 원본 버텍스/인덱스 뷰를 반환한다.
FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshLOD* LOD = MeshAsset ? MeshAsset->GetLOD(0) : nullptr;
	if (!LOD || LOD->Vertices.empty())
	{
		return {};
	}

	FMeshDataView View;
	View.VertexData = LOD->Vertices.data();
	View.VertexCount = static_cast<uint32>(LOD->Vertices.size());
	View.Stride = sizeof(FSkeletalVertex);
	View.IndexData = LOD->Indices.data();
	View.IndexCount = static_cast<uint32>(LOD->Indices.size());
	return View;
}

// 에디터 디버그 렌더링용으로 현재 본 계층을 선으로 표시한다.
void USkeletalMeshComponent::ContributeVisuals(FScene& Scene) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!ShouldDisplayBones() || !MeshAsset || MeshAsset->Skeleton.Bones.empty())
	{
		return;
	}

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	const FMatrix& ComponentWorld = GetWorldMatrix();

	// 계산된 현재 포즈가 있으면 그것을 쓰고, 없으면 bind pose로 fallback한다.
	auto GetBoneMatrix = [&](int32 BoneIndex) -> FMatrix
	{
		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ComponentSpaceTransforms.size()))
		{
			return ComponentSpaceTransforms[BoneIndex].ToMatrix();
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
		const bool bSelected = BoneIndex == SelectedBoneIndex || ParentIndex == SelectedBoneIndex;
		Scene.AddForegroundDebugLine(ParentWorld, BoneWorld, bSelected ? FColor::Yellow() : FColor(190, 205, 215, 255));
	}
}

// 특정 본의 로컬 공간 트랜스폼을 반환한다.
const FTransform* USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	return IsValidBoneIndex(BoneIndex) ? &BoneSpaceTransforms[BoneIndex] : nullptr;
}

// 특정 본의 컴포넌트 공간 트랜스폼을 반환한다.
const FTransform* USkeletalMeshComponent::GetBoneComponentSpaceTransform(int32 BoneIndex) const
{
	return IsValidBoneIndex(BoneIndex) ? &ComponentSpaceTransforms[BoneIndex] : nullptr;
}

void USkeletalMeshComponent::SetSelectedBoneIndex(int32 BoneIndex)
{
	SelectedBoneIndex = IsValidBoneIndex(BoneIndex) ? BoneIndex : -1;
}

// 본 로컬 트랜스폼을 갱신하고 스키닝/렌더 프록시를 다시 계산하도록 표시한다.
void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	if (!IsValidBoneIndex(BoneIndex))
	{
		return;
	}

	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size())
	{
		InitializeBoneTransformsFromSkeleton();
	}

	FTransform NormalizedTransform = NewTransform;
	NormalizedTransform.Rotation.Normalize();
	BoneSpaceTransforms[BoneIndex] = NormalizedTransform;
	RefreshBoneTransforms();
	UpdateSkinnedMeshObject();
	MarkWorldBoundsDirty();
}

// 컴포넌트 공간 트랜스폼을 부모 본 기준 로컬 트랜스폼으로 변환해 적용한다.
bool USkeletalMeshComponent::SetBoneComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	if (!IsValidBoneIndex(BoneIndex))
	{
		return false;
	}

	const FSkeleton& Skeleton = GetSkeletalMesh()->GetSkeletalMeshAsset()->Skeleton;
	FMatrix LocalMatrix = NewTransform.ToMatrix();
	const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
	if (IsValidBoneIndex(ParentIndex))
	{
		const FMatrix ParentMatrix = ParentIndex < static_cast<int32>(ComponentSpaceTransforms.size())
			? ComponentSpaceTransforms[ParentIndex].ToMatrix()
			: Skeleton.Bones[ParentIndex].GlobalBindPose;
		LocalMatrix = LocalMatrix * ParentMatrix.GetInverse();
	}

	SetBoneLocalTransform(BoneIndex, TransformFromMatrix(LocalMatrix));
	return true;
}

// 로컬 본 배열에서 컴포넌트 공간 행렬과 스키닝 팔레트를 갱신한다.
// Convert component-space rotation to local-space without an Euler roundtrip.
bool USkeletalMeshComponent::SetBoneComponentSpaceRotation(int32 BoneIndex, const FQuat& NewComponentRotation)
{
	if (!IsValidBoneIndex(BoneIndex))
	{
		return false;
	}

	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size())
	{
		InitializeBoneTransformsFromSkeleton();
	}

	if (BoneIndex >= static_cast<int32>(BoneSpaceTransforms.size()))
	{
		return false;
	}

	const FSkeleton& Skeleton = GetSkeletalMesh()->GetSkeletalMeshAsset()->Skeleton;
	FQuat NewRotation = NewComponentRotation.GetNormalized();
	const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
	if (IsValidBoneIndex(ParentIndex))
	{
		FQuat ParentComponentRotation = ParentIndex < static_cast<int32>(ComponentSpaceTransforms.size())
			? ComponentSpaceTransforms[ParentIndex].Rotation
			: Skeleton.Bones[ParentIndex].GlobalBindPose.ToQuat();
		ParentComponentRotation.Normalize();

		// Relative = ParentInverse * Total. Since A * B means B first, ParentInverse must be on the left.
		NewRotation = (ParentComponentRotation.Inverse() * NewRotation).GetNormalized();
	}

	FTransform LocalTransform = BoneSpaceTransforms[BoneIndex];
	LocalTransform.Rotation = NewRotation;
	SetBoneLocalTransform(BoneIndex, LocalTransform);
	return true;
}

// Refresh component-space transforms and skinning palette from local bone transforms.
void USkeletalMeshComponent::RefreshBoneTransforms()
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	const int32 BoneCount = Skeleton ? static_cast<int32>(Skeleton->Bones.size()) : 0;
	if (BoneCount == 0)
	{
		BoneSpaceTransforms.clear();
		ComponentSpaceTransforms.clear();
		ComponentSpaceMatrices.clear();
		SkinningMatrices.clear();
		RequiredBones.clear();
		SelectedBoneIndex = -1;
		bRequiredBonesUpdated = true;
		bPoseDirty = false;
		bSkinningDirty = false;
		return;
	}

	if (BoneSpaceTransforms.size() != BoneCount)
	{
		InitializeBoneTransformsFromSkeleton();
		return;
	}

	ComponentSpaceTransforms.resize(BoneCount);
	ComponentSpaceMatrices.assign(BoneCount, FMatrix::Identity);
	SkinningMatrices.assign(BoneCount, FMatrix::Identity);
	RequiredBones.resize(BoneCount);

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		RequiredBones[BoneIndex] = BoneIndex;
		
		const int32 ParentIndex = Skeleton->Bones[BoneIndex].ParentIndex;
		const FTransform& LocalTransform = BoneSpaceTransforms[BoneIndex];

		if (ParentIndex >= 0 && ParentIndex < BoneIndex)
		{
			const FTransform& ParentTransform = ComponentSpaceTransforms[ParentIndex];

			// Component = Parent * Local (FTransform Composition)
			FTransform& OutTransform = ComponentSpaceTransforms[BoneIndex];
			
			// Rotation: ParentWorld * Local
			OutTransform.Rotation = (ParentTransform.Rotation * LocalTransform.Rotation).GetNormalized();
			
			// Location: ParentWorldLoc + ParentWorldRot * (ParentWorldScale * LocalLoc)
			FVector ScaledLocalLoc = ParentTransform.Scale * LocalTransform.Location;
			OutTransform.Location = ParentTransform.Location + ParentTransform.Rotation.RotateVector(ScaledLocalLoc);
			
			// Scale: ParentWorldScale * LocalScale
			OutTransform.Scale = ParentTransform.Scale * LocalTransform.Scale;

			// Skinning Matrix (Can be skewed)
			ComponentSpaceMatrices[BoneIndex] = LocalTransform.ToMatrix() * ComponentSpaceMatrices[ParentIndex];
		}
		else
		{
			ComponentSpaceTransforms[BoneIndex] = LocalTransform;
			ComponentSpaceMatrices[BoneIndex] = LocalTransform.ToMatrix();
		}

		SkinningMatrices[BoneIndex] = Skeleton->Bones[BoneIndex].InverseBindPose * ComponentSpaceMatrices[BoneIndex];
	}

	SelectedBoneIndex = IsValidBoneIndex(SelectedBoneIndex) ? SelectedBoneIndex : -1;
	bRequiredBonesUpdated = true;
	bPoseDirty = false;
	bSkinningDirty = false;
	bBoundsDirty = true;
}

// 새 스켈레탈 메시를 설정하고 bind pose 기반 본 배열을 초기화한다.
void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	USkinnedMeshComponent::SetSkeletalMesh(InSkeletalMesh);
	InitializeBoneTransformsFromSkeleton();
	UpdateSkinnedMeshObject();
}

// 스켈레탈 메시 컴포넌트 전용 본 포즈와 표시 옵션을 직렬화한다.
void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	USkinnedMeshComponent::Serialize(Ar);
	Ar << BoneSpaceTransforms;
	Ar << bForceRefPose;
	Ar << bEnableSkeletonUpdate;
	Ar << RootBoneTranslation;
	Ar << bShowBoneNames;

	if (Ar.IsLoading())
	{
		if (BoneSpaceTransforms.empty())
		{
			InitializeBoneTransformsFromSkeleton();
		}
		else
		{
			RefreshBoneTransforms();
		}
	}
}

void USkeletalMeshComponent::PostDuplicate()
{
	USkinnedMeshComponent::PostDuplicate();
	RefreshBoneTransforms();
}

// 에디터 프로퍼티 패널에 노출할 스켈레탈 메시/본 표시 옵션을 구성한다.
void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();
	UMeshComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });
	OutProps.push_back({ "CPU Skinning", EPropertyType::Bool, &bCPUSkinning });
	OutProps.push_back({ "Hide Skin", EPropertyType::Bool, &bHideSkin });
	OutProps.push_back({ "Force Ref Pose", EPropertyType::Bool, &bForceRefPose });
	OutProps.push_back({ "Enable Skeleton Update", EPropertyType::Bool, &bEnableSkeletonUpdate });
	OutProps.push_back({ "Root Bone", EPropertyType::Vec3, &RootBoneTranslation });
	OutProps.push_back({ "Selected Bone Index", EPropertyType::Int, &SelectedBoneIndex, -1.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });
	OutProps.push_back({ "Show Bone Names", EPropertyType::Bool, &bShowBoneNames });

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(MaterialSlots.size()); ++SlotIndex)
	{
		OutProps.push_back({ "Element " + std::to_string(SlotIndex), EPropertyType::MaterialSlot, &MaterialSlots[SlotIndex] });
	}
}

// 에디터에서 변경된 프로퍼티에 따라 메시, 머티리얼, 본 포즈 상태를 갱신한다.
void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		SetSkeletalMesh(IsNonePath(SkeletalMeshPath) ? nullptr : FSkeletalMeshManager::LoadSkeletalMesh(SkeletalMeshPath));
		return;
	}

	if (std::strncmp(PropertyName, "Element ", 8) == 0)
	{
		const int32 SlotIndex = std::atoi(&PropertyName[8]);
		if (SlotIndex >= 0 && SlotIndex < static_cast<int32>(MaterialSlots.size()))
		{
			const FString& MaterialPath = MaterialSlots[SlotIndex].Path;
			SetMaterial(SlotIndex, IsNonePath(MaterialPath) ? nullptr : FMaterialManager::Get().GetOrCreateMaterial(MaterialPath));
		}
		return;
	}

	if (std::strcmp(PropertyName, "Force Ref Pose") == 0
		|| std::strcmp(PropertyName, "Enable Skeleton Update") == 0
		|| std::strcmp(PropertyName, "Root Bone") == 0)
	{
		MarkSkeletalPoseDirty();
		return;
	}

	if (std::strcmp(PropertyName, "Selected Bone Index") == 0
		|| std::strcmp(PropertyName, "Display Bones") == 0
		|| std::strcmp(PropertyName, "Show Bone Names") == 0)
	{
		SetSelectedBoneIndex(SelectedBoneIndex);
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

// 본 포즈 변경으로 스키닝, 바운드, 렌더 프록시가 다시 계산되어야 함을 표시한다.
void USkeletalMeshComponent::MarkSkeletalPoseDirty()
{
	bRequiredBonesUpdated = false;
	bPoseDirty = true;
	bSkinningDirty = true;
	bBoundsDirty = true;
	MarkProxyDirty(EDirtyFlag::Mesh);
	MarkWorldBoundsDirty();
}

// 메시의 bind pose를 기반으로 로컬 본 트랜스폼 배열을 초기화한다.
void USkeletalMeshComponent::InitializeBoneTransformsFromSkeleton()
{
	BoneSpaceTransforms.clear();
	ComponentSpaceTransforms.clear();
	ComponentSpaceMatrices.clear();
	SkinningMatrices.clear();
	RequiredBones.clear();

	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	if (!Skeleton || Skeleton->Bones.empty())
	{
		SelectedBoneIndex = -1;
		bRequiredBonesUpdated = true;
		bPoseDirty = false;
		bSkinningDirty = false;
		return;
	}

	BoneSpaceTransforms.reserve(Skeleton->Bones.size());
	for (const FBoneInfo& Bone : Skeleton->Bones)
	{
		BoneSpaceTransforms.push_back(TransformFromMatrix(Bone.LocalBindPose));
	}

	RefreshBoneTransforms();
}

// 현재 메시의 스켈레톤 범위 안에 있는 본 인덱스인지 확인한다.
bool USkeletalMeshComponent::IsValidBoneIndex(int32 BoneIndex) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	return MeshAsset
		&& BoneIndex >= 0
		&& BoneIndex < static_cast<int32>(MeshAsset->Skeleton.Bones.size());
}
