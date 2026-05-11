#pragma once

#include "Object/Object.h"
#include "Serialization/Archive.h"
#include "SkeletalMeshAsset.h"

// ============================================================================
// USkeletalMesh
//
// SkeletalMesh 리소스를 엔진 UObject 시스템에서 다루기 위한 에셋 래퍼.
//
// 역할:
// - FSkeletalMesh 원본 데이터 포인터 보관
// - SkeletalMesh 머티리얼 슬롯 보관
// - Bake 파일 또는 에셋 파일 저장/로드 시 Serialize 수행
// - Section.MaterialSlotName을 SkeletalMaterials 배열의 MaterialIndex로 매핑
//
// - FSkeletalMesh*
//   실제 스켈레탈 메시 원본 데이터
//
// - SkeletalMaterials
//   이 SkeletalMesh가 사용하는 머티리얼 슬롯 배열.
// ============================================================================
class USkeletalMesh : public UObject
{
public:
    DECLARE_CLASS(USkeletalMesh, UObject)

    USkeletalMesh() = default;
    ~USkeletalMesh() override;

    void Serialize(FArchive& Ar) override;

    const FString& GetAssetPathFileName() const;

    void           SetSkeletalMeshAsset(FSkeletalMesh* InMesh);
    FSkeletalMesh* GetSkeletalMeshAsset() const;

    void                           SetSkeletalMaterials(TArray<FStaticMaterial>&& InMaterials);
    const TArray<FStaticMaterial>& GetSkeletalMaterials() const;

private:
    void RebuildSectionMaterialIndices();

private:
    FSkeletalMesh*          SkeletalMeshAsset = nullptr;
    TArray<FStaticMaterial> SkeletalMaterials;
};
