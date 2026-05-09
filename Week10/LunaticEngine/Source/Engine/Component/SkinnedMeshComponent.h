#pragma once

#include "Component/MeshComponent.h"
#include "Core/PropertyTypes.h"

class UMaterial;
class USkeletalMesh;

class USkinnedMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

	USkinnedMeshComponent() = default;
	~USkinnedMeshComponent() override = default;

	USkeletalMesh* SkinnedAsset = nullptr;
	FString SkinnedMeshAssetPath = "None";

	TArray<UMaterial*> OverrideMaterials;
	TArray<FMaterialSlot> MaterialSlots;

	TArray<FTransform> ComponentSpaceTransforms;
	TArray<FMatrix> SkinningMatrices;

	// int32 CurrentLODIndex = 0;
	// int32 ForceLODIndex = -1;
	// int32 MinLODIndex = 0;

	bool bCPUSkinning = true;
	bool bSkinningDirty = true;
	bool bPoseDirty = true;
	bool bBoundsDirty = true;

	bool bDisplayBones = false;
	bool bHideSkin = false;

	USkeletalMesh* GetSkinnedAsset() const { return SkinnedAsset; }
	const FString& GetSkinnedMeshAssetPath() const { return SkinnedMeshAssetPath; }

	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }
	UMaterial* GetMaterial(int32 ElementIndex) const;
	int32 GetMaterialSlotCount() const { return static_cast<int32>(MaterialSlots.size()); }
	FMaterialSlot* GetMaterialSlot(int32 ElementIndex);
	const FMaterialSlot* GetMaterialSlot(int32 ElementIndex) const;
	void EnsureMaterialSlotsForEditing();

	const TArray<FTransform>& GetComponentSpaceTransforms() const { return ComponentSpaceTransforms; }
	const TArray<FMatrix>& GetSkinningMatrices() const { return SkinningMatrices; }

	bool IsCPUSkinningEnabled() const { return bCPUSkinning; }
	bool IsSkinningDirty() const { return bSkinningDirty; }
	bool IsPoseDirty() const { return bPoseDirty; }
	bool IsSkinnedBoundsDirty() const { return bBoundsDirty; }
	bool ShouldDisplayBones() const { return bDisplayBones; }
	bool ShouldHideSkin() const { return bHideSkin; }

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

protected:
	void MarkSkinnedMeshDataDirty();
	void ResolveMaterialSlotsFromPaths();
};
