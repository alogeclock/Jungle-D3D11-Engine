#pragma once
#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Object/FName.h"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"
#include "Mesh/StaticMeshAsset.h"

constexpr int32 MAX_BONE_INFLUENCES = 4;

struct FSkinVertex
{
	FVector Pos;
	FVector Normal;
	FVector4 Color;
	FVector2 Tex;
	FVector4 Tangent;
	uint32 BoneIndices[MAX_BONE_INFLUENCES];
	float BoneWeights[MAX_BONE_INFLUENCES];
};

struct FBone
{
	FName Name;
	int32 ParentIndex = -1;
	FTransform BindPoseLocal;
	FMatrix InverseBindPoseGlobal;
};

struct FSkeleton
{
	TArray<FBone> Bones;
	int32 FindBoneIndex(FName Name) const;
};

// FSkeletalMeshLOD — 단일 LOD의 정점/인덱스/섹션
// FStaticMeshSection을 재사용해 머티리얼 매핑을 StaticMesh와 동일한 방식으로 처리한다
// RenderBuffer는 GPU 업로드 단계에서 추가 예정
struct FSkeletalMeshLOD
{
	TArray<FSkinVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;

	FVector BoundsCenter{ 0, 0, 0 };
	FVector BoundsExtent{ 0, 0, 0 };
	bool bBoundsValid = false;

	void CacheBounds();
};

struct FSkeletalMesh
{
	FString PathFileName;
	TArray<FSkeletalMeshLOD> LODModels;
	FSkeleton Skeleton;

	void Serialize(FArchive& Ar);
};
