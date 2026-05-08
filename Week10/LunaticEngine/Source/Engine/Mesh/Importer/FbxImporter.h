#pragma once
#include <string>
#include "Core/CoreTypes.h"

struct FStaticMesh;
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

};
