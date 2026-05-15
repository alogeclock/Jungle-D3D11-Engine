#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/SkeletonAsset.h"
#include "Mesh/StaticMeshAsset.h"
#include "Render/Types/VertexTypes.h"

#include <fbxsdk.h>

struct FImportOptions;

class FEditorFbxImporter
{
public:
	struct FImportedSkeletalMesh
	{
		FString MeshName;
		FMatrix MeshBindGlobal = FMatrix::Identity;
		TArray<FVertexPNCTBW> Vertices;
		TArray<uint32> Indices;
		TArray<FSkeletalMeshSection> Sections;
		TArray<FSkeletalMaterial> Materials;
	};

private:
	struct FMaterialInfo
	{
		FString Name;
		FVector DiffuseColor;
		FString TexturePath;
		FString NormalTexturePath;
	};

public:
	static bool Import(const FString& FilePath);
	static bool ImportStatic(const FString& FilePath, const FImportOptions* Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
	static bool DiscoverMeshNames(const FString& FilePath, TArray<FString>& OutMeshNames);

private:
	static bool Parse(FbxScene* Scene);
	static bool Convert();

	static void CollectNodes(FbxNode* Node, int32 depth, TArray<FbxNode*>& OutNodes);
	static void CollectMaterials(FbxScene* Scene);

	static void ParseBone(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& OutNodeToIndex);
	static void ParseSkin(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& NodeToIndex);

	// Helper
	static int32 GetMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex);
	static int32 FindNearestParentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& NodeToIndex);

	static void TriangulateScene(FbxScene* Scene);
	static FString ConvertToMat(const FMaterialInfo* MaterialInfo);

	static void GenerateTangents(uint32 TriIndices[]);
	static void BuildTangentsForVertexRange(const uint32 VertexStart);

public:
	// Temporary data structures for parsing
	static TArray<FVertexPNCTBW> Vertices;
	static TArray<uint32> Indices;
	static TArray<FBone> Bones;
	static TArray<FSkeletalMeshSection> Sections;
	static TArray<FImportedSkeletalMesh> ImportedSkeletalMeshes;
	static TArray<FMaterialInfo> MtlInfos;
	static TMap<FbxSurfaceMaterial*, int32> MaterialToSlotIndex;
	static TArray<FVector> TangentSums;
	static TArray<FVector> BitangentSums;
	static FString CurrentSourcePath;

};
