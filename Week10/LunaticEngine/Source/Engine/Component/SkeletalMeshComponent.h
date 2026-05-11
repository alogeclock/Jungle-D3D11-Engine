#pragma once

#include "Component/SkinnedMeshComponent.h"

class FPrimitiveSceneProxy;
class FScene;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// Mesh Buffer & AABB
	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;

	// Scene Proxy & Debug Visuals
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void ContributeVisuals(FScene& Scene) const override;

	// Asset
	USkeletalMesh* GetSkeletalMeshAsset() const { return SkeletalMeshAsset; }
	const FString& GetSkeletalMeshAssetPath() const { return SkeletalMeshAssetPath; }

	// Bone Transform
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform);
	bool SetBoneComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform);
	void RefreshBoneTransforms();
	void EnsureMaterialSlotsForEditing();

	// Getter & Setter
	const TArray<FTransform>& GetBoneSpaceTransforms() const { return BoneSpaceTransforms; }
	const TArray<int32>& GetRequiredBones() const { return RequiredBones; }
	const FTransform* GetBoneLocalTransform(int32 BoneIndex) const;
	const FTransform* GetBoneComponentSpaceTransform(int32 BoneIndex) const;

	bool AreRequiredBonesUpdated() const { return bRequiredBonesUpdated; }
	bool IsForceRefPoseEnabled() const { return bForceRefPose; }
	bool IsSkeletonUpdateEnabled() const { return bEnableSkeletonUpdate; }

	const FVector& GetRootBoneTranslation() const { return RootBoneTranslation; }
	int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }
	bool ShouldShowSkeleton() const { return bShowSkeleton; }
	bool ShouldShowBoneNames() const { return bShowBoneNames; }

	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
	void SetSelectedBoneIndex(int32 BoneIndex);

private:
	void SyncSkinnedAssetPath();
	void MarkSkeletalPoseDirty();
	void CacheLocalBounds();
	void InitializeBoneTransformsFromSkeleton();
	bool IsValidBoneIndex(int32 BoneIndex) const;

private:
	USkeletalMesh* SkeletalMeshAsset = nullptr;
	FString SkeletalMeshAssetPath = "None";

	TArray<FTransform> BoneSpaceTransforms; // 부모 본에 대한 로컬 트랜스폼
	TArray<int32> RequiredBones;

	bool bRequiredBonesUpdated = true;
	bool bForceRefPose = false;
	bool bEnableSkeletonUpdate = true;

	FVector RootBoneTranslation = FVector::ZeroVector;

	int32 SelectedBoneIndex = -1;
	bool bShowSkeleton = true;
	bool bShowBoneNames = false;

	// bool bUseRefPoseOnInitAnimation = true;
	// bool bEnableAnimation = false;
	// bool bPauseAnimation = false;
	// float GlobalAnimationRate = 1.0f;
};
