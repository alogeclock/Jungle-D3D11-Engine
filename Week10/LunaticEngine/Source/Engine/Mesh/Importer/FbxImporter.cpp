#include "FbxImporter.h"
#include "Core/Log.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/MaterialManager.h"
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
		const FbxAMatrix NodeTransform = Node->EvaluateGlobalTransform();
		const FbxVector4 TransformedOrigin = NodeTransform.MultT(FbxVector4(0.0, 0.0, 0.0));
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

				point = NodeTransform.MultT(point);
				normal = NodeTransform.MultT(normal) - TransformedOrigin;
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

	if (OutMesh.Vertices.empty() || OutMesh.Indices.empty())
	{
		UE_LOG_CATEGORY(FbxImporter, Error, "FBX import produced empty mesh: %s", FilePath.c_str());
		Scene->Destroy();
		Manager->Destroy();
		return false;
	}

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

	UE_LOG_CATEGORY(
		FbxImporter,
		Info,
		"FBX static mesh imported: Vertices=%d Indices=%d Triangles=%d BoundsCenter=(%.3f, %.3f, %.3f) BoundsExtent=(%.3f, %.3f, %.3f)",
		static_cast<int32>(OutMesh.Vertices.size()),
		static_cast<int32>(OutMesh.Indices.size()),
		static_cast<int32>(OutMesh.Indices.size() / 3),
		OutMesh.BoundsCenter.X,
		OutMesh.BoundsCenter.Y,
		OutMesh.BoundsCenter.Z,
		OutMesh.BoundsExtent.X,
		OutMesh.BoundsExtent.Y,
		OutMesh.BoundsExtent.Z);

	Scene->Destroy();
	Manager->Destroy();
	return !OutMesh.Vertices.empty() && !OutMesh.Indices.empty();
}

//ImportSkeletalMesh 
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/Skeleton.h"

namespace
{
	// 한 control point에 영향 주는 (boneIdx, weight) 페어를 모아둠
	// 마지막에 top-4 highest weights만 남기고 정규화
	struct FRawSkinInfluence
	{
		TArray<int32> BoneIndices;
		TArray<float> Weights;
	};

}

static FTransform FbxLocalTransformToFTransform(FbxNode* Node)
{
	FbxAMatrix Local = Node->EvaluateLocalTransform();

	FbxVector4 Transform = Local.GetT();
	FbxQuaternion Quat = Local.GetQ();
	FbxVector4 Scale = Local.GetS();

	// Axis translat: FBX based Y-up RH To LH Z-up
	// temporary keep to original without swap
	FTransform Out;
	Out.Location = FVector((float)Transform[0], (float)Transform[1], (float)Transform[2]);
	Out.Rotation = FQuat((float)Quat[0], (float)Quat[1], (float)Quat[2], (float)Quat[3]);
	Out.Scale = FVector((float)Scale[0], (float)Scale[1], (float)Scale[2]);
	return Out;
}
static void CollectBonesRecursive(FbxNode* Node, int32 ParentIndex, FReferenceSkeleton& OutRef,
	TMap<FbxNode*, int32>& OutNodeToIndex)
{
	if (!Node) return;

	FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	const bool bIsBone = Attr && Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton;

	int32 MyIndex = ParentIndex;
	if (bIsBone)
	{
		FMeshBoneInfo Info;
		Info.Name = FName(Node->GetName());
		Info.ParentIndex = ParentIndex;

		OutRef.Bones.push_back(Info);
		OutRef.RefBonePose.push_back(FbxLocalTransformToFTransform(Node));
		MyIndex = (int32)OutRef.Bones.size() - 1;

		OutNodeToIndex[Node] = MyIndex;
	}
	for (int32 i = 0; i < Node->GetChildCount(); ++i)
	{
		CollectBonesRecursive(Node->GetChild(i), MyIndex, OutRef, OutNodeToIndex);
	}
}
static void BuildReferenceSkeleton(FbxScene* Scene, FReferenceSkeleton& OutRef, TMap<FbxNode*, int32>& OutNodeToIndex)
{
	OutRef.Empty();
	OutNodeToIndex.clear();
	if (!Scene || !Scene->GetRootNode())
		return;

	CollectBonesRecursive(Scene->GetRootNode(), -1, OutRef, OutNodeToIndex);
	OutRef.RefBasesInvMatrix.resize(OutRef.GetNum(), FMatrix::Identity);
	OutRef.RebuildRefBasesInvMatrix();
}
static void ImportSkeletalNodeRecursive(FbxNode* Node, const FFbxImportOptions& Options, const TMap<FbxNode*, int32>& BoneNodeMap, 
	FSkeletalMeshLOD& OutLOD, TArray<FRawSkinInfluence>& OutControlPointInfluences, TArray<TArray<uint32>>& OutCPToVertex)
{
	if (!Node)
		return;
	if (FbxMesh* Mesh = Node->GetMesh())
	{  // Control point 개수만큼 영향 슬롯 미리 잡기
		const int32 ControlPointCount = Mesh->GetControlPointsCount();
		const int32 BaseCPOffset = (int32)OutControlPointInfluences.size();
		OutControlPointInfluences.resize(BaseCPOffset + ControlPointCount);
		OutCPToVertex.resize(BaseCPOffset + ControlPointCount);

		// extract skin Data for control point
		const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
		for (int32 s = 0; s < SkinCount; s++)
		{
			FbxSkin* Skin = (FbxSkin*)Mesh->GetDeformer(s, FbxDeformer::eSkin);
			const int32 ClusterCount = Skin->GetClusterCount();
			for (int32 c = 0; c < ClusterCount; ++c)
			{
				FbxCluster* Cluster = Skin->GetCluster(c);
				FbxNode* LinkNode = Cluster->GetLink();
				if (!LinkNode) 
					continue;

				auto It = BoneNodeMap.find(LinkNode);
				if (It == BoneNodeMap.end())
					continue;
				const int32 BoneIdx = It->second;

				const int32 IndexCount = Cluster->GetControlPointIndicesCount();
				const int* CPIndices = Cluster->GetControlPointIndices();
				const double* CPWeights = Cluster->GetControlPointWeights();

				for (int32 j = 0; j < IndexCount; ++j)
				{
					const int32 CPLocal = CPIndices[j];
					const float W = (float)CPWeights[j];
					if (W <= 0.0f) continue;

					FRawSkinInfluence& Infl = OutControlPointInfluences[BaseCPOffset + CPLocal];
					Infl.BoneIndices.push_back(BoneIdx);
					Infl.Weights.push_back(W);
				}
			}
		}
		//UV setname
		const char* UVSetName = nullptr;
		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		if (UVSetNames.GetCount() > 0)
		{
			UVSetName = UVSetNames[0];
		}
		// Polygon ->Vertex emission 
		for (int32 Poly = 0; Poly < Mesh->GetPolygonCount(); ++Poly)
		{
			if (Mesh->GetPolygonSize(Poly) != 3)
				continue;
			uint32 TriIndices[3];
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 CPLocal = Mesh->GetPolygonVertex(Poly, Corner);
				FbxVector4 point = Mesh->GetControlPointAt(CPLocal);
				FbxVector4 normal; Mesh->GetPolygonVertexNormal(Poly, Corner, normal); normal.Normalize();
				FbxVector2 UV(0, 0); bool bUnmapped = false;
				if (UVSetName)
					Mesh->GetPolygonVertexUV(Poly, Corner, UVSetName, UV, bUnmapped);
				// 스키닝은 mesh-local에서 일어나므로 NodeTransform 적용 X
				FSkinVertex v{};

				v.Pos = FVector((float)point[0], (float)point[1], (float)point[2]) * Options.Scale;
				v.Normal = FVector((float)normal[0], (float)normal[1], (float)normal[2]).Normalized();
				v.Color = FVector4(1, 1, 1, 1);
				v.Tex = FVector2((float)UV[0], 1.0f - (float)UV[1]);
				v.Tangent = FVector4(1, 0, 0, 1);
				for (int k = 0; k < MAX_BONE_INFLUENCES; ++k) 
				{ 
					v.BoneIndices[k] = 0;
					v.BoneWeights[k] = 0.0f; 
				}
				const uint32 OutIdx = (uint32)OutLOD.Vertices.size();
				OutLOD.Vertices.push_back(v);
				OutCPToVertex[BaseCPOffset + CPLocal].push_back(OutIdx);
				TriIndices[Corner] = OutIdx;
			}
			OutLOD.Indices.push_back(TriIndices[0]);
			OutLOD.Indices.push_back(TriIndices[2]);
			OutLOD.Indices.push_back(TriIndices[1]);
		}
	}
	for (int32 i = 0; i < Node->GetChildCount(); ++i)
	{
		ImportSkeletalNodeRecursive(Node->GetChild(i), Options, BoneNodeMap,
			OutLOD, OutControlPointInfluences, OutCPToVertex);
	}
}
//Control point effects -> output vertx 4-bone fan-out + normalize
static void FanOutSkinWeights(const TArray<FRawSkinInfluence>& CPInfluences,const TArray<TArray<uint32>>& CPToVertex,FSkeletalMeshLOD& LOD)
{
	for (size_t cp = 0; cp < CPInfluences.size(); ++cp)
	{
		const FRawSkinInfluence& Infl = CPInfluences[cp];
		if (Infl.BoneIndices.empty())
			continue;
	//weights sellect
		TArray<int32> Order(Infl.Weights.size());
		for (int32 i = 0; i < (int32)Order.size(); i++)
			Order[i] = i;
		std::sort(Order.begin(), Order.end(),
			[&](int32 a, int32 b) { return Infl.Weights[a] > Infl.Weights[b]; });
		
		const int32 Take = std::min<int32>((int32)Order.size(), MAX_BONE_INFLUENCES);
		float Sum = 0.0f;
		uint32 BIdx[MAX_BONE_INFLUENCES] = { 0, 0, 0, 0 };
		float  BWgt[MAX_BONE_INFLUENCES] = { 0, 0, 0, 0 };
		for (int32 k = 0; k < Take; k++)
		{
			BIdx[k] = (uint32)Infl.BoneIndices[Order[k]];
			BWgt[k] = Infl.Weights[Order[k]];
			Sum += BWgt[k];
		}
		if (Sum > 1e-6f)
		{
			for (int32 k = 0; k < MAX_BONE_INFLUENCES; ++k)
				BWgt[k] /= Sum;
		}
		else
		{
			BIdx[0] = 0;
			BWgt[0] = 1.0f;
		}
		for (uint32 vIdx : CPToVertex[cp])
		{
			FSkinVertex& Vertex = LOD.Vertices[vIdx];
			for (int32 k = 0; k < MAX_BONE_INFLUENCES; ++k)
			{
				Vertex.BoneIndices[k] = BIdx[k];
				Vertex.BoneWeights[k] = BWgt[k];
			}
		}
	}
}
bool FFbxImporter::ImportSkeletalMesh(const FString& FilePath, const FFbxImportOptions& Options,
	FSkeletalMesh& OutMesh,	FReferenceSkeleton& OutSkeleton, TArray<FStaticMaterial>& OutMaterials)
{
	OutMesh = FSkeletalMesh();
	OutSkeleton.Empty();
	OutMaterials.clear();

	FbxManager* Manager = FbxManager::Create();
	FbxIOSettings* IOSetting = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSetting);

	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(FilePath.c_str(), -1, Manager->GetIOSettings()))
	{
		UE_LOG_CATEGORY(FbxImporter, Error, "Failed to open FBX: %s", FilePath.c_str());
		Importer->Destroy(); Manager->Destroy();
		return false;
	}
	FbxScene* Scene = FbxScene::Create(Manager, "FbxSkeletalMeshScene");
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
	// Skeleton Build
	TMap<FbxNode*, int32> BoneNodeMap;
	BuildReferenceSkeleton(Scene, OutSkeleton, BoneNodeMap);
	// Mesh + Skin extract
	OutMesh.PathFileName = FilePath;
	OutMesh.LODModels.resize(1);
	FSkeletalMeshLOD& LOD = OutMesh.LODModels[0];

	TArray<FRawSkinInfluence> CPInfluences;
	TArray<TArray<uint32>> CPToVertex;
	ImportSkeletalNodeRecursive(Scene->GetRootNode(), Options, BoneNodeMap,	LOD, CPInfluences, CPToVertex);


	if (LOD.Vertices.empty() || LOD.Indices.empty())
	{
		UE_LOG_CATEGORY(FbxImporter, Error, "FBX skeletal import produced empty mesh: %s", FilePath.c_str());
		Scene->Destroy();
		Manager->Destroy();
		return false;
	}
	//Skin weight fan - out 
	if (OutSkeleton.GetNum() > 0)
	{
		FanOutSkinWeights(CPInfluences, CPToVertex, LOD);
	}
	else
	{
		UE_LOG_CATEGORY(FbxImporter, Warning, "FBX has no skeleton nodes — wrapping with 1-bone identity.");
		OutSkeleton.Allocate(1);
		OutSkeleton.Bones[0] = { FName{"root"}, -1};
		OutSkeleton.RefBonePose[0] = FTransform();
		OutSkeleton.RebuildRefBasesInvMatrix();
		for(FSkinVertex& vtx: LOD.Vertices)
		{
			vtx.BoneIndices[0] = 0;
			vtx.BoneWeights[0] = 1.0f;
		}
	}
	// Section / Material 
	LOD.Sections.resize(1);
	LOD.Sections[0].MaterialIndex = 0;
	LOD.Sections[0].MaterialSlotName = "None";
	LOD.Sections[0].FirstIndex = 0;
	LOD.Sections[0].NumTriangles = (uint32)(LOD.Indices.size() / 3);

	FStaticMaterial Default;
	Default.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");
	Default.MaterialSlotName = "None";
	OutMaterials.push_back(Default);
	LOD.CacheBounds();

	UE_LOG_CATEGORY(FbxImporter, Info,
		"FBX skeletal mesh imported: Vertices=%d Triangles=%d Bones=%d",
		(int32)LOD.Vertices.size(), (int32)(LOD.Indices.size() / 3), OutSkeleton.GetNum());

	Scene->Destroy(); Manager->Destroy();
	return true;
}

