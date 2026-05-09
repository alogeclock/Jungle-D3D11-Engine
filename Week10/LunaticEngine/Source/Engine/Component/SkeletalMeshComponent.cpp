#include "Component/SkeletalMeshComponent.h"

#include "Object/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Serialization/Archive.h"

#include <cstring>
#include <string>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	// 스켈레탈 메시 경로가 비어 있는지 확인합니다.
	bool IsNonePath(const FString& Path)
	{
		return Path.empty() || Path == "None";
	}

	// 비어 있는 스켈레탈 메시 경로를 에디터에서 쓰는 None 값으로 정규화합니다.
	void NormalizeAssetPath(FString& Path)
	{
		if (Path.empty())
		{
			Path = "None";
		}
	}

	// 기존 SceneComponent 직렬화 방식에 맞춰 본 트랜스폼을 멤버 단위로 직렬화합니다.
	void SerializeTransforms(FArchive& Ar, TArray<FTransform>& Transforms)
	{
		uint32 TransformCount = static_cast<uint32>(Transforms.size());
		Ar << TransformCount;

		if (Ar.IsLoading())
		{
			Transforms.resize(TransformCount);
		}

		for (FTransform& Transform : Transforms)
		{
			Ar << Transform.Location;
			Ar << Transform.Rotation;
			Ar << Transform.Scale;
		}
	}
}

// 유효한 본 인덱스라면 본의 로컬 트랜스폼을 반환합니다.
const FTransform* USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	return (BoneIndex >= 0 && BoneIndex < static_cast<int32>(BoneSpaceTransforms.size()))
		? &BoneSpaceTransforms[BoneIndex]
		: nullptr;
}

// 유효한 본 인덱스라면 본의 컴포넌트 공간 트랜스폼을 반환합니다.
const FTransform* USkeletalMeshComponent::GetBoneComponentSpaceTransform(int32 BoneIndex) const
{
	return (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ComponentSpaceTransforms.size()))
		? &ComponentSpaceTransforms[BoneIndex]
		: nullptr;
}

// 스켈레탈 메시 에셋 참조를 상위 스킨드 에셋 필드에 동기화합니다.
void USkeletalMeshComponent::SyncSkinnedAssetPathFromSkeletalMesh()
{
	NormalizeAssetPath(SkeletalMeshAssetPath);
	SkinnedMeshAssetPath = SkeletalMeshAssetPath;
	SkinnedAsset = SkeletalMeshAsset;
}

// 본 포즈, 스키닝, 바운드, 렌더 메시 데이터가 더티 플래그를 표시합니다.
void USkeletalMeshComponent::MarkSkeletalPoseDirty()
{
	bRequiredBonesUpdated = false;
	bPoseDirty = true;
	bSkinningDirty = true;
	bBoundsDirty = true;

	MarkProxyDirty(EDirtyFlag::Mesh);
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		SyncSkinnedAssetPathFromSkeletalMesh();
	}

	USkinnedMeshComponent::Serialize(Ar);

	Ar << SkeletalMeshAssetPath;
	SerializeTransforms(Ar, BoneSpaceTransforms);
	Ar << bForceRefPose;
	Ar << bEnableSkeletonUpdate;
	Ar << RootBoneTranslation;
	Ar << bShowSkeleton;
	Ar << bShowBoneNames;

	if (Ar.IsLoading())
	{
		NormalizeAssetPath(SkeletalMeshAssetPath);

		if (IsNonePath(SkeletalMeshAssetPath) && !IsNonePath(SkinnedMeshAssetPath))
		{
			SkeletalMeshAssetPath = SkinnedMeshAssetPath;
		}

		SyncSkinnedAssetPathFromSkeletalMesh();
		MarkSkeletalPoseDirty();
	}
}

void USkeletalMeshComponent::PostDuplicate()
{
	USkinnedMeshComponent::PostDuplicate();

	NormalizeAssetPath(SkeletalMeshAssetPath);
	if (IsNonePath(SkeletalMeshAssetPath) && !IsNonePath(SkinnedMeshAssetPath))
	{
		SkeletalMeshAssetPath = SkinnedMeshAssetPath;
	}

	SyncSkinnedAssetPathFromSkeletalMesh();
	MarkSkeletalPoseDirty();
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();

	UMeshComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Skeletal Mesh", EPropertyType::String, &SkeletalMeshAssetPath });
	OutProps.push_back({ "CPU Skinning", EPropertyType::Bool, &bCPUSkinning });
	OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });
	OutProps.push_back({ "Hide Skin", EPropertyType::Bool, &bHideSkin });
	OutProps.push_back({ "Force Ref Pose", EPropertyType::Bool, &bForceRefPose });
	OutProps.push_back({ "Enable Skeleton Update", EPropertyType::Bool, &bEnableSkeletonUpdate });
	OutProps.push_back({ "Root Bone Translation", EPropertyType::Vec3, &RootBoneTranslation });
	OutProps.push_back({ "Selected Bone Index", EPropertyType::Int, &SelectedBoneIndex, -1.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Show Skeleton", EPropertyType::Bool, &bShowSkeleton });
	OutProps.push_back({ "Show Bone Names", EPropertyType::Bool, &bShowBoneNames });

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(MaterialSlots.size()); ++SlotIndex)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(SlotIndex);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[SlotIndex];
		OutProps.push_back(Desc);
	}
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	USkinnedMeshComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		NormalizeAssetPath(SkeletalMeshAssetPath);

		// TODO: 1번 팀원의 USkeletalMesh loader/manager가 들어오면 여기에서 path를 asset으로 resolve한다.
		if (IsNonePath(SkeletalMeshAssetPath))
		{
			SkeletalMeshAsset = nullptr;
			SkinnedAsset = nullptr;
		}

		SyncSkinnedAssetPathFromSkeletalMesh();
		EnsureMaterialSlotsForEditing();
		MarkSkeletalPoseDirty();
		return;
	}

	if (std::strcmp(PropertyName, "Force Ref Pose") == 0
		|| std::strcmp(PropertyName, "Enable Skeleton Update") == 0
		|| std::strcmp(PropertyName, "Root Bone Translation") == 0)
	{
		MarkSkeletalPoseDirty();
		return;
	}

	if (std::strcmp(PropertyName, "Selected Bone Index") == 0
		|| std::strcmp(PropertyName, "Show Skeleton") == 0
		|| std::strcmp(PropertyName, "Show Bone Names") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}
