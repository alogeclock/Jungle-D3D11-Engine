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
static void CountNodesRecursive(FbxNode* Node, FFbxImportStats& Stats)
{
	if (!Node) return;

	++Stats.NodeCount;

	FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	if (Attr && Attr->GetAttributeType() == FbxNodeAttribute::eMesh)
	{
		++Stats.MeshCount;

		FbxMesh* Mesh = Node->GetMesh();
		if (Mesh)
		{
			Stats.VertexCount += Mesh->GetControlPointsCount();
			Stats.TriangleCount += Mesh->GetPolygonCount();
		}
	}

	for (int32 i = 0; i < Node->GetChildCount(); ++i)
	{
		CountNodesRecursive(Node->GetChild(i), Stats);
	}
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
		CountNodesRecursive(Scene->GetRootNode(), OutStats);

		UE_LOG_CATEGORY(
			FbxImporter,
			Info,
			"FBX Summary: Nodes=%d Meshes=%d Vertices=%d Triangles=%d",
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

static void ImportNode(FbxNode* Node, const FFbxImportOptions& Options, FStaticMesh& OutMesh)
{
	if (!Node)
		return;
	if (FbxMesh* Mesh = Node->GetMesh())
	{
		const char* UVSetName = nullptr;
		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		if (UVSetNames.GetCount() > 0)
		{
			UVSetName = UVSetNames[0];
		}


		for (int32 Poly = 0; Poly < Mesh->GetPolygonCount(); Poly++)
		{
			if (Mesh->GetPolygonSize(Poly) != 3)
			{
				continue;
			}
			uint32 TriIndices[3];


			for (int32 Corner = 0; Corner < 3; Corner++)
			{
				const int32 ControlPointIndex = Mesh->GetPolygonVertex(Poly, Corner);

				FbxVector4 point = Mesh->GetControlPointAt(ControlPointIndex);
				FbxVector4 normal;
				Mesh->GetPolygonVertexNormal(Poly, Corner, normal);
				normal.Normalize();

				FbxVector2 UV(0.0, 0.0);
				bool bUnmapped = false;

				if (UVSetName)
				{
					Mesh->GetPolygonVertexUV(Poly, Corner, UVSetName, UV, bUnmapped);
				}

				FNormalVertex V;
				V.pos = FVector((float)point[0], (float)point[1], (float)point[2]) * Options.Scale;
				V.normal = FVector((float)normal[0], (float)normal[1], (float)normal[2]).Normalized();
				V.color = FVector4(1, 1, 1, 1);
				V.tex = FVector2((float)UV[0], 1.0f - (float)UV[1]);
				V.tangent = FVector4(1, 0, 0, 1);

				TriIndices[Corner] = static_cast<uint32>(OutMesh.Vertices.size());
				OutMesh.Vertices.push_back(V);
			}
			OutMesh.Indices.push_back(TriIndices[0]);
			OutMesh.Indices.push_back(TriIndices[2]);
			OutMesh.Indices.push_back(TriIndices[1]);
		}
	}
	for (int32 i = 0; i < Node->GetChildCount(); ++i)
	{
		ImportNode(Node->GetChild(i), Options, OutMesh);
	}
}
bool FFbxImporter::ImportStaticMesh(const FString& FilePath, const FFbxImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	OutMesh = FStaticMesh();
	OutMaterials.clear();

	FbxManager* Manager = FbxManager::Create();
	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSettings);

	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(FilePath.c_str(), -1, Manager->GetIOSettings()))
	{
		Importer->Destroy();
		Manager->Destroy();
		return false;
	}



	FbxScene* Scene = FbxScene::Create(Manager, "FbxStaticMeshTest");
	const bool bImported = Importer->Import(Scene);
	Importer->Destroy();
	if (!bImported)
	{
		Scene->Destroy();
		Manager->Destroy();
		return false;
	}
	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, true);



	ImportNode(Scene->GetRootNode(), Options, OutMesh);

	FStaticMeshSection Section;
	Section.MaterialIndex = 0;
	Section.MaterialSlotName = "None";
	Section.FirstIndex = 0;
	Section.NumTriangles = static_cast<uint32>(OutMesh.Indices.size() / 3);
	OutMesh.Sections.push_back(Section);
	FStaticMaterial DefaultMat;
	DefaultMat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");
	DefaultMat.MaterialSlotName = "None";

	OutMaterials.push_back(DefaultMat);

	OutMesh.PathFileName = FilePath;
	OutMesh.CacheBounds();

	Scene->Destroy();
	Manager->Destroy();
	return !OutMesh.Vertices.empty() && !OutMesh.Indices.empty();
}

