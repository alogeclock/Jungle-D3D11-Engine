#pragma once
#include <string>
#include "Core/CoreTypes.h"

struct FSkeletalMesh;
struct FReferenceSkeleton;
struct FStaticMaterial;
struct FFbxImportOptions
{
	float Scale = 1.0f;
	bool bImportMaterials = true;
	bool bImportTextures = true;
	bool bGenerateTangents = true;
	bool bCombineMeshes = true;
};

struct FFbxImportStats
{
	int32 NodeCount = 0;
	int32 MeshCount = 0;
	int32 VertexCount = 0;
	int32 TriangleCount = 0;
};

struct FFbxImporter
{
	static bool SmokeTest();
	static bool LoadSceneSummary(const FString& FilePath, FFbxImportStats& OutStats);
	static bool ImportStaticMesh(const FString& FilePath, const FFbxImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
// FBX에서 정점/인덱스 + 본 hierarchy + skin weights를 모두 추출.
// OutSkeleton: FBX의 SkeletonRoot 트리에서 빌드 (DFS 순서, ParentIndex < ChildIndex)
//              RefBonePose는 LclTransform(bind state)에서 추출
//              RefBasesInvMatrix는 RebuildRefBasesInvMatrix로 자동 계산
// OutMesh: 정점별 BoneIndices[4]/BoneWeights[4]가 정규화된 상태로 채워짐
	static bool ImportSkeletalMesh(const FString& FilePath, const FFbxImportOptions& Options,
		FSkeletalMesh& OutMesh, FReferenceSkeleton& OutSkeleton, TArray<FStaticMaterial>& OutMaterials);
};
