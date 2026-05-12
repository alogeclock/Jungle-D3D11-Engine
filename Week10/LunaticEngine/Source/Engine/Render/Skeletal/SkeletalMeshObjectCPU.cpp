#include "SkeletalMeshObjectCPU.h"
#include <algorithm>
#include <cmath>

#include "Mesh/SkeletalMeshAsset.h"
FSkeletalMeshObjectCPU::FSkeletalMeshObjectCPU(const FSkeletalMesh* InSource, ID3D11Device* InDevice)
	: Source(InSource), Device(InDevice)
{
	if (!Source || Source->LODModels.empty()) return;
	const FSkeletalMeshLOD& LOD = Source->LODModels[0];

	// 정적 인덱스 복사
	SkinnedMeshData.Indices = LOD.Indices;

	// 정점 슬롯 미리 할당
	SkinnedMeshData.Vertices.resize(LOD.Vertices.size());

	MeshBuffer = std::make_unique<FMeshBuffer>();
}

void FSkeletalMeshObjectCPU::Update(const TArray<FMatrix>& InSkinningMatrices)
{
	if (!Source || Source->LODModels.empty() || !Device) return;
	const FSkeletalMeshLOD& LOD = Source->LODModels[0];

	if (SkinnedMeshData.Vertices.size() != LOD.Vertices.size())
		SkinnedMeshData.Vertices.resize(LOD.Vertices.size());

	//Cpu skinning kenel
	//every vertex: SkinningPos = Σ (Wk *(skinMatrix[BoneIdx]*V.Pos)
	//			    SkinnedNrm = Σ Wk * (SkinMatrix[BoneIdxk].Rot * V.Normal) 
// Skining Cpu

	for (size_t v = 0; v < LOD.Vertices.size(); v++)
	{
		const FSkeletalVertex& In = LOD.Vertices[v];

		FVector Pos(0, 0, 0), Normal(0, 0, 0);
		float TotalWeight = 0.0f;
		for (int i = 0; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; i++)
		{
			const float Weight = In.BoneWeights[i];
			if (Weight <= 0.0f)
				continue;
			const uint32 BoneIdx = In.BoneIndices[i];
			if (BoneIdx >= (uint32)InSkinningMatrices.size())
				continue;

			const FMatrix& Matrix = InSkinningMatrices[BoneIdx];
			Pos = Pos + Matrix.TransformPositionWithW(In.Pos) * Weight;
			Normal = Normal + Matrix.TransformVector(In.Normal) * Weight;
			TotalWeight += Weight;
		}


		if (TotalWeight <= 0.0f)
		{
			Pos = In.Pos;
			Normal = In.Normal;
		}
		else if (std::abs(TotalWeight - 1.0f) > 1.e-4f)
		{
			const float InvWeight = 1.0f / TotalWeight;
			Pos = Pos * InvWeight;
			Normal = Normal * InvWeight;
		}

		Normal = Normal.Normalized();

		FVertexPNCTT& Out = SkinnedMeshData.Vertices[v];
		Out.Position = Pos;
		Out.Normal = Normal;
		Out.Color = In.Color;
		Out.UV = In.UV[0];
		Out.Tangent = In.Tangent;
	}


	ID3D11DeviceContext* Context = nullptr;
	Device->GetImmediateContext(&Context);
	if (Context)
	{
		FVertexBuffer& VB = MeshBuffer->GetVertexBuffer();
		uint32 TotalSize = static_cast<uint32>(SkinnedMeshData.Vertices.size() * sizeof(FVertexPNCTT));
		
		if (!VB.GetBuffer())
		{

			VB.Create(Device, SkinnedMeshData.Vertices.data(), (uint32)SkinnedMeshData.Vertices.size(), TotalSize, sizeof(FVertexPNCTT), true);
			
	
			MeshBuffer->GetIndexBuffer().Create(Device, SkinnedMeshData.Indices.data(), (uint32)SkinnedMeshData.Indices.size(), (uint32)SkinnedMeshData.Indices.size() * sizeof(uint32));
		}
		else
		{

			VB.Update(Context, SkinnedMeshData.Vertices.data(), TotalSize);
		}
		Context->Release();
	}
}
