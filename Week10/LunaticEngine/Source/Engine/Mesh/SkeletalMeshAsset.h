#pragma once
#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Object/FName.h"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"
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
struct FSkeletalMeshSection
{
	int32 MaterialIndex = -1;
	FString MaterialSlotName;
	uint32 FirstIndex = 0;
	uint32 NumTriangles = 0;
};
struct FSkeletalMesh
{
	FString PathFileName;
	TArray<FSkinVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FSkeletalMeshSection> Sections;
	FSkeleton Skeleton;

	FVector BoundsCenter{ 0,0,0 };
	FVector BoundsExtent{ 0,0,0 };
	bool bBoundsValid = false;
	
	void CacheBounds();
	void Serialize(FArchive& Ar);
};