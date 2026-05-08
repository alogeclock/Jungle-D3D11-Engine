#include "FbxImporter.h"
#include "Core/Log.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include <fbxsdk.h>



bool FFbxImporter::SmokeTest()
{
	FbxManager* Manager = FbxManager::Create();
	if (!Manager)
	{
		return false;
	}

	Manager->Destroy();
	return true;
}

bool FFbxImporter::LoadSceneSummary(const FString& FilePath, FFbxImportStats& OutStats)
{
	OutStats = FFbxImportStats();
	FbxManager* Manager = FbxManager::Create();
	if (!Manager)
	{
		UE_LOG_CATEGORY(FbxImporter, Error, "Failed to create FbxManager");
		return false;
	}
	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSettings);

	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(FilePath.c_str(), -1, Manager->GetIOSettings()))
	{
		UE_LOG_CATEGORY(FbxImporter, Error, "Failed to open FBX: %s", FilePath.c_str());
		Importer->Destroy();
		Manager->Destroy();
		return false;
	}

	FbxScene* Scene = FbxScene::Create(Manager, "ImportedScene");
	const bool bImported = Importer->Import(Scene);

	if (bImported)
	{
		FbxGeometryConverter Converter(Manager);
		Converter.Triangulate(Scene, true);
		UE_LOG_CATEGORY(
			FbxImporter,
			Info,
			"FBX Summary: NodeCount=%d MeshCount=%d VertexCount=%d TriangleCount=%d",
			OutStats.NodeCount,
			OutStats.MeshCount,
			OutStats.VertexCount,
			OutStats.TriangleCount
		);
	}

	Scene->Destroy();
	Importer->Destroy();
	Manager->Destroy();

	return bImported;
}


bool FFbxImporter::ImportStaticMesh(const FString& FilePath, const FFbxImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	return false;
}
