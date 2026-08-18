#pragma once

#include "Component/MeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Mesh/SkeletalMesh.h"
#include "Render/Skeletal/SkeletalMeshObject.h"   // unique_ptr<FSkeletalMeshObject> complete type 요구

#include <memory>

class UMaterial;
class FPrimitiveSceneProxy;
struct FSkeletalAnimationClip;

// SkeletalMesh 자산을 들고, "외부에서 주어진 BoneSpaceTransforms"를
// component-space matrix + skinning matrix로 가공하는 책임까지만 진다.
// Animation 평가는 자식 USkeletalMeshComponent가 담당.

class USkinnedMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

	USkinnedMeshComponent() = default;
	// std::unique_ptr<FSkeletalMeshObject>가 complete type을 요구하므로 .cpp에서 정의.
	~USkinnedMeshComponent() override;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;
	bool LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult) override;
	void UpdateWorldAABB() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
	USkeletalMesh* GetSkinnedAsset() const { return SkeletalMesh; }

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }
	void EnsureMaterialSlotsForEditing();
	int32 GetMaterialSlotCount() const { return static_cast<int32>(MaterialSlots.size()); }
	FMaterialSlot* GetMaterialSlot(int32 ElementIndex);
	const FMaterialSlot* GetMaterialSlot(int32 ElementIndex) const;

	const FString& GetSkeletalMeshPath() const { return SkeletalMeshPath; }
	const FString& GetSkinnedMeshAssetPath() const { return SkeletalMeshPath; }

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// 파생 클래스가 BoneSpaceTransforms를 채운다 (RefreshBoneTransforms에서)
	// FillComponentSpaceTransforms로 ComponentSpace + Skinning matrix 빌드
	// SkeletalMeshObject에 SkinningMatrices 전달
	virtual void RefreshBoneTransforms();
	void FillComponentSpaceTransforms();

	const TArray<FTransform>& GetBoneSpaceTransforms() const { return BoneSpaceTransforms; }
	const TArray<FTransform>& GetComponentSpaceTransforms() const { return ComponentSpaceTransforms; }
	const TArray<FMatrix>& GetComponentSpaceMatrices() const { return ComponentSpaceMatrices; }
	const TArray<FMatrix>& GetSkinningMatrices() const { return SkinningMatrices; }
	const TArray<FVector>& GetBoneSpaceMirrorSigns() const { return BoneSpaceMirrorSigns; }

	bool IsCPUSkinningEnabled() const { return bCPUSkinning; }
	bool IsSkinningDirty() const { return bSkinningDirty; }
	bool IsPoseDirty() const { return bPoseDirty; }
	bool IsSkinnedBoundsDirty() const { return bBoundsDirty; }
	bool ShouldHideSkin() const { return bHideSkin; }
	bool ShouldDisplayBones() const { return bDisplayBones; }
	void SetDisplayBones(bool bDisplay);
	void SetRenderLOD(uint32 LODIndex);


	void SetMorphTarget(const FString& MorphName, float Value);
	float GetMorphTarget(const FString& MorphName) const;
	void ClearMorphTargets();
	void ApplyAnimationFloatCurves(const FSkeletalAnimationClip& Clip, float TimeSeconds);
	// 본 포즈 파이프라인 invariant 검증.
	// 1) RefPose 입력 시 모든 SkinningMatrix가 Identity 인지
	// 2) RTTI 체인이 USkeletalMeshComponent → USkinnedMeshComponent → UMeshComponent 인지
	// EngineLoop::Init에서 한 번 호출 권장. 실패 시 check() 트립.
	static bool SelfTest();

protected:
	USkeletalMesh* SkeletalMesh = nullptr;
	FString SkeletalMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	TArray<FMaterialSlot> MaterialSlots;

	bool bCPUSkinning = true;
	bool bSkinningDirty = true;
	bool bPoseDirty = true;
	bool bBoundsDirty = true;
	bool bHideSkin = false;
	bool bDisplayBones = false;

	float BoundsScale = 1.0f;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;

	// UE는 ComponentSpaceTransforms를 [2] 더블 버퍼로 갖지만, 단일 스레드라 단일 배열.
	// 추후 RT/GT 분리 시 [2] + read/write index 변수로 확장.
	TArray<FTransform> BoneSpaceTransforms;        // 부모 로컬 [BoneCount] - scale magnitude only, mirror signs are stored separately
	TArray<FVector> BoneSpaceMirrorSigns;          // 부모 로컬 mirror/sign [BoneCount], hidden from editor scale UI
	TArray<FTransform> ComponentSpaceTransforms;   // 컴포넌트 공간 [BoneCount] - positive scale for editor/UI
	TArray<FMatrix> ComponentSpaceMatrices;     // 컴포넌트 공간 [BoneCount]
	TArray<FMatrix> SkinningMatrices;           // CS * RefBasesInvMatrix [BoneCount]
	std::unique_ptr<FSkeletalMeshObject> MeshObject;

	TArray<float> MorphTargetWeights;
	bool bMorphTargetsDirty = true;

	int32 FindMorphTargetIndex(const FString& MorphName) const;
	void EnsureMorphTargetWeights();

	void SetSkeletalMeshInternal(USkeletalMesh* InMesh, bool bBuildInitialSkinning, bool bUpdateRenderState);
	void FinalizeSkeletalMeshRenderState();
	void CacheLocalBounds();
	void InvalidateSkinnedMeshState(bool bClearPose);
	void UpdateSkinnedMeshObject();
};
