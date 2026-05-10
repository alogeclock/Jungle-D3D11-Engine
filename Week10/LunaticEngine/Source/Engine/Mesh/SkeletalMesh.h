#pragma once

#include "Object/Object.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/StaticMeshAsset.h" // FStaticMaterial
#include "Serialization/Archive.h"

// USkeletalMesh — FSkeletalMesh 에셋을 소유하는 UObject 래퍼
// UStaticMesh와 동일한 슬롯 모델(FStaticMaterial 배열)을 사용한다
class USkeletalMesh : public UObject
{
public:
	DECLARE_CLASS(USkeletalMesh, UObject)

	USkeletalMesh() = default;
	~USkeletalMesh() override;

	void Serialize(FArchive& Ar);

	const FString& GetAssetPathFileName() const;

	void SetSkeletalMeshAsset(FSkeletalMesh* InMesh);
	FSkeletalMesh* GetSkeletalMeshAsset() const { return SkeletalMeshAsset; }

	void SetSkeletalMaterials(TArray<FStaticMaterial>&& InMaterials);
	const TArray<FStaticMaterial>& GetSkeletalMaterials() const { return SkeletalMaterials; }

private:
	FSkeletalMesh* SkeletalMeshAsset = nullptr;
	TArray<FStaticMaterial> SkeletalMaterials;
};
