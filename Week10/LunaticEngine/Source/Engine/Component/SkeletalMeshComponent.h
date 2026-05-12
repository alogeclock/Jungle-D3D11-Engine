#pragma once

#include "Component/SkinnedMeshComponent.h"

class FScene;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override;

	FMeshDataView GetMeshDataView() const override;
	void ContributeVisuals(FScene& Scene) const override;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// Asset
	USkeletalMesh* GetSkeletalMeshAsset() const { return GetSkeletalMesh(); }
	const FString& GetSkeletalMeshAssetPath() const { return GetSkeletalMeshPath(); }

	// Bone Transform
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform);
	bool SetBoneComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform);
	bool SetBoneComponentSpaceRotation(int32 BoneIndex, const FQuat& NewComponentRotation);
	void RefreshBoneTransforms() override;

	// Getter & Setter
	const TArray<int32>& GetRequiredBones() const { return RequiredBones; }
	const FTransform* GetBoneLocalTransform(int32 BoneIndex) const;
	const FTransform* GetBoneComponentSpaceTransform(int32 BoneIndex) const;

	bool AreRequiredBonesUpdated() const { return bRequiredBonesUpdated; }
	bool IsForceRefPoseEnabled() const { return bForceRefPose; }
	bool IsSkeletonUpdateEnabled() const { return bEnableSkeletonUpdate; }

	const FVector& GetRootBoneTranslation() const { return RootBoneTranslation; }
	int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }
	bool ShouldShowBoneNames() const { return bShowBoneNames; }

	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh) override;
	void SetSelectedBoneIndex(int32 BoneIndex);

private:
	void MarkSkeletalPoseDirty();
	void InitializeBoneTransformsFromSkeleton();
	bool IsValidBoneIndex(int32 BoneIndex) const;

private:
	TArray<int32> RequiredBones;

	bool bRequiredBonesUpdated = true;
	bool bForceRefPose = false;
	bool bEnableSkeletonUpdate = true;

	FVector RootBoneTranslation = FVector::ZeroVector;

	int32 SelectedBoneIndex = -1;
	bool bShowBoneNames = false;
};
