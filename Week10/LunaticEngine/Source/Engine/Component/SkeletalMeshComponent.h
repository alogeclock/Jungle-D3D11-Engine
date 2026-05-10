#pragma once

#include "Component/MeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMesh.h"

class UMaterial;
class FPrimitiveSceneProxy;

namespace json { class JSON; }

// ============================================================
// USkeletalMeshComponent — 월드 배치용 스켈레탈 메시 컴포넌트
// ============================================================
// 현재는 빈 껍데기 단계.
// USkeletalMesh 에셋을 들고, 머티리얼 오버라이드 슬롯과 Bounds만 정상 동작.
// GPU RenderBuffer / Skinning / LineTrace는 후속 단계에서 채운다.
class USkeletalMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, UMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;
	bool LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult) override;
	void UpdateWorldAABB() const override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }
	void EnsureMaterialSlotsForEditing();
	int32 GetMaterialSlotCount() const { return static_cast<int32>(MaterialSlots.size()); }
	FMaterialSlot* GetMaterialSlot(int32 ElementIndex);
	const FMaterialSlot* GetMaterialSlot(int32 ElementIndex) const;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetSkeletalMeshPath() const { return SkeletalMeshPath; }

private:
	void CacheLocalBounds();

	USkeletalMesh* SkeletalMesh = nullptr;
	FString SkeletalMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	TArray<FMaterialSlot> MaterialSlots;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
