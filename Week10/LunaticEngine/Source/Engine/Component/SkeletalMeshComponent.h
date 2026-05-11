#pragma once

#include "Component/SkinnedMeshComponent.h"

class FPrimitiveSceneProxy;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;
	void UpdateWorldAABB() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	USkeletalMesh* GetSkeletalMeshAsset() const { return SkeletalMeshAsset; }
	const FString& GetSkeletalMeshAssetPath() const { return SkeletalMeshAssetPath; }

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
	void EnsureMaterialSlotsForEditing();

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

private:
	void SyncSkinnedAssetPathFromSkeletalMesh();
	void MarkSkeletalPoseDirty();
	void CacheLocalBounds();

private:
	USkeletalMesh* SkeletalMeshAsset = nullptr;
	FString SkeletalMeshAssetPath = "None";

	TArray<FTransform> BoneSpaceTransforms; // 부모 본에 대한 로컬 트랜스폼
	TArray<int32> RequiredBones;

	FVector CachedLocalCenter = { 0.0f, 0.0f, 0.0f };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;

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
