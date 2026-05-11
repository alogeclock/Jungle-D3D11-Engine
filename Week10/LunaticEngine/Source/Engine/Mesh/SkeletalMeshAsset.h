#pragma once
#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Object/FName.h"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"
#include "StaticMeshAsset.h"
#include "Skeleton.h"
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

struct FSkeletalMeshLOD
{
	TArray<FSkinVertex>        Vertices;
	TArray<uint32>             Indices;
	TArray<FStaticMeshSection> Sections;

	FVector BoundsCenter{ 0, 0, 0 };
	FVector BoundsExtent{ 0, 0, 0 };
	bool    bBoundsValid = false;

	void CacheBounds();
};


// FSkeletalMesh — 정점 자산만 보관.
struct FSkeletalMesh
{
	FString PathFileName;
	TArray<FSkeletalMeshLOD> LODModels;

	void Serialize(FArchive& Ar);
};
