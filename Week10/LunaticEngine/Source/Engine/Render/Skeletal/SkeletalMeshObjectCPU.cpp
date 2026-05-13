#include "SkeletalMeshObjectCPU.h"
#include <algorithm>
#include <cmath>

#include "Mesh/SkeletalMeshAsset.h"

namespace
{
	FVector NormalizeOrFallback(const FVector& Value, const FVector& Fallback)
	{
		return Value.IsNearlyZero(1.0e-6f) ? Fallback : Value.Normalized();
	}

	FVector OrthogonalizeTangent(const FVector& Tangent, const FVector& Normal)
	{
		const FVector N = NormalizeOrFallback(Normal, FVector(0.0f, 0.0f, 1.0f));
		FVector T = Tangent - N * Tangent.Dot(N);
		if (T.IsNearlyZero(1.0e-6f))
		{
			const FVector Candidate = std::fabs(N.Z) < 0.999f ? FVector(0.0f, 0.0f, 1.0f) : FVector(1.0f, 0.0f, 0.0f);
			T = Candidate - N * Candidate.Dot(N);
		}
		return T.Normalized();
	}

	void ApplyMorphDelta(FSkeletalVertex& Vertex, const FMorphTargetDelta& Delta, float Scale)
	{
		Vertex.Pos += Delta.PositionDelta * Scale;
		Vertex.Normal += Delta.NormalDelta * Scale;
		Vertex.Tangent += Delta.TangentDelta * Scale;
	}

	void ApplyShapeDeltasScaled(const FMorphTargetShape& Shape, float Scale, TArray<FSkeletalVertex>& InOutVertices)
	{
		if (std::abs(Scale) <= 1.0e-4f)
		{
			return;
		}

		for (const FMorphTargetDelta& Delta : Shape.Deltas)
		{
			if (Delta.VertexIndex < InOutVertices.size())
			{
				ApplyMorphDelta(InOutVertices[Delta.VertexIndex], Delta, Scale);
			}
		}
	}

	void ApplyInterpolatedShapeDeltas(const FMorphTargetShape& A, const FMorphTargetShape& B, float Alpha, TArray<FSkeletalVertex>& InOutVertices)
	{
		size_t AIndex = 0;
		size_t BIndex = 0;

		while (AIndex < A.Deltas.size() || BIndex < B.Deltas.size())
		{
			const bool bUseA =
				BIndex >= B.Deltas.size() ||
				(AIndex < A.Deltas.size() && A.Deltas[AIndex].VertexIndex < B.Deltas[BIndex].VertexIndex);
			const bool bUseB =
				AIndex >= A.Deltas.size() ||
				(BIndex < B.Deltas.size() && B.Deltas[BIndex].VertexIndex < A.Deltas[AIndex].VertexIndex);

			if (bUseA)
			{
				const FMorphTargetDelta& Delta = A.Deltas[AIndex++];
				if (Delta.VertexIndex < InOutVertices.size())
				{
					ApplyMorphDelta(InOutVertices[Delta.VertexIndex], Delta, 1.0f - Alpha);
				}
				continue;
			}

			if (bUseB)
			{
				const FMorphTargetDelta& Delta = B.Deltas[BIndex++];
				if (Delta.VertexIndex < InOutVertices.size())
				{
					ApplyMorphDelta(InOutVertices[Delta.VertexIndex], Delta, Alpha);
				}
				continue;
			}

			const FMorphTargetDelta& DeltaA = A.Deltas[AIndex++];
			const FMorphTargetDelta& DeltaB = B.Deltas[BIndex++];
			const uint32 VertexIndex = DeltaA.VertexIndex;
			if (VertexIndex >= InOutVertices.size())
			{
				continue;
			}

			FSkeletalVertex& Vertex = InOutVertices[VertexIndex];
			Vertex.Pos += DeltaA.PositionDelta + (DeltaB.PositionDelta - DeltaA.PositionDelta) * Alpha;
			Vertex.Normal += DeltaA.NormalDelta + (DeltaB.NormalDelta - DeltaA.NormalDelta) * Alpha;
			Vertex.Tangent += DeltaA.TangentDelta + (DeltaB.TangentDelta - DeltaA.TangentDelta) * Alpha;
		}
	}

	void ApplyMorphLODByWeight(const FMorphTargetLOD& MorphLOD, float RuntimeWeight, TArray<FSkeletalVertex>& InOutVertices)
	{
		if (MorphLOD.Shapes.empty())
		{
			return;
		}

		const float FbxWeight = std::clamp(RuntimeWeight, 0.0f, 1.0f) * 100.0f;
		const FMorphTargetShape& FirstShape = MorphLOD.Shapes.front();
		if (MorphLOD.Shapes.size() == 1 || FbxWeight <= FirstShape.FullWeight)
		{
			const float Denom = std::max(std::abs(FirstShape.FullWeight), 1.0e-4f);
			ApplyShapeDeltasScaled(FirstShape, std::clamp(FbxWeight / Denom, 0.0f, 1.0f), InOutVertices);
			return;
		}

		for (int32 ShapeIndex = 1; ShapeIndex < static_cast<int32>(MorphLOD.Shapes.size()); ++ShapeIndex)
		{
			const FMorphTargetShape& PrevShape = MorphLOD.Shapes[ShapeIndex - 1];
			const FMorphTargetShape& NextShape = MorphLOD.Shapes[ShapeIndex];
			if (FbxWeight > NextShape.FullWeight)
			{
				continue;
			}

			const float Denom = std::max(NextShape.FullWeight - PrevShape.FullWeight, 1.0e-4f);
			const float Alpha = std::clamp((FbxWeight - PrevShape.FullWeight) / Denom, 0.0f, 1.0f);
			ApplyInterpolatedShapeDeltas(PrevShape, NextShape, Alpha, InOutVertices);
			return;
		}

		ApplyShapeDeltasScaled(MorphLOD.Shapes.back(), 1.0f, InOutVertices);
	}
}
FSkeletalMeshObjectCPU::FSkeletalMeshObjectCPU(const FSkeletalMesh* InSource, ID3D11Device* InDevice)
	: Source(InSource), Device(InDevice)
{
	if (!Source || Source->LODModels.empty()) return;
	const FSkeletalMeshLOD& LOD = Source->LODModels[CurrentLOD];

	// 정적 인덱스 복사
	SkinnedMeshData.Indices = LOD.Indices;

	// 정점 슬롯 미리 할당
	SkinnedMeshData.Vertices.resize(LOD.Vertices.size());

	MeshBuffer = std::make_unique<FMeshBuffer>();
}




void FSkeletalMeshObjectCPU::SetLOD(uint32 LODIndex)
{
	if (!Source || Source->LODModels.empty())
	{
		return;
	}

	const uint32 ClampedLOD = std::min<uint32>(LODIndex, static_cast<uint32>(Source->LODModels.size() - 1));
	if (CurrentLOD == ClampedLOD)
	{
		return;
	}

	CurrentLOD = ClampedLOD;
	const FSkeletalMeshLOD& LOD = Source->LODModels[CurrentLOD];
	SkinnedMeshData.Indices = LOD.Indices;
	SkinnedMeshData.Vertices.resize(LOD.Vertices.size());
	bHasSkinnedLocalBounds = false;

	if (MeshBuffer)
	{
		MeshBuffer->Release();
	}
}

void FSkeletalMeshObjectCPU::Update(const TArray<FMatrix>& InSkinningMatrices, const TArray<float>* MorphTargetWeights)
{

	if (!Source || Source->LODModels.empty() || !Device) return;
	const uint32 LODIndex = std::min<uint32>(CurrentLOD, static_cast<uint32>(Source->LODModels.size() - 1));
	const FSkeletalMeshLOD& LOD = Source->LODModels[LODIndex];

	if (SkinnedMeshData.Vertices.size() != LOD.Vertices.size())
		SkinnedMeshData.Vertices.resize(LOD.Vertices.size());
	if (MorphedVertices.size() != LOD.Vertices.size())
		MorphedVertices.resize(LOD.Vertices.size());

	std::copy(LOD.Vertices.begin(), LOD.Vertices.end(), MorphedVertices.begin());

	ApplyMorphTargets(LODIndex, MorphTargetWeights, MorphedVertices);
	TArray<FMatrix> NormalMatrices;
	NormalMatrices.reserve(InSkinningMatrices.size());
	for (const FMatrix& SkinningMatrix : InSkinningMatrices)
	{
		NormalMatrices.push_back(SkinningMatrix.GetInverse().GetTransposed());
	}

	bHasSkinnedLocalBounds = false;
	FVector LocalMin(0.0f, 0.0f, 0.0f);
	FVector LocalMax(0.0f, 0.0f, 0.0f);
	for (size_t v = 0; v < LOD.Vertices.size(); v++)
	{
		const FSkeletalVertex& In = MorphedVertices[v];

		FVector Pos(0, 0, 0), Normal(0, 0, 0), Tangent(0, 0, 0);
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
			Normal = Normal + NormalMatrices[BoneIdx].TransformVector(In.Normal) * Weight;
			Tangent = Tangent + Matrix.TransformVector(FVector(In.Tangent.X, In.Tangent.Y, In.Tangent.Z)) * Weight;
			TotalWeight += Weight;
		}


		if (TotalWeight <= 0.0f)
		{
			Pos = In.Pos;
			Normal = In.Normal;
			Tangent = FVector(In.Tangent.X, In.Tangent.Y, In.Tangent.Z);
		}
		else if (std::abs(TotalWeight - 1.0f) > 1.e-4f)
		{
			const float InvWeight = 1.0f / TotalWeight;
			Pos = Pos * InvWeight;
			Normal = Normal * InvWeight;
			Tangent = Tangent * InvWeight;
		}

		Normal = NormalizeOrFallback(Normal, In.Normal);
		Tangent = OrthogonalizeTangent(Tangent, Normal);

		FVertexPNCTT& Out = SkinnedMeshData.Vertices[v];
		Out.Position = Pos;
		Out.Normal = Normal;
		Out.Color = In.Color;
		Out.UV = In.UV[0];
		Out.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, In.Tangent.W);

		if (!bHasSkinnedLocalBounds)
		{
			LocalMin = LocalMax = Pos;
			bHasSkinnedLocalBounds = true;
		}
		else
		{
			LocalMin.X = std::min(LocalMin.X, Pos.X);
			LocalMin.Y = std::min(LocalMin.Y, Pos.Y);
			LocalMin.Z = std::min(LocalMin.Z, Pos.Z);
			LocalMax.X = std::max(LocalMax.X, Pos.X);
			LocalMax.Y = std::max(LocalMax.Y, Pos.Y);
			LocalMax.Z = std::max(LocalMax.Z, Pos.Z);
		}
	}

	if (bHasSkinnedLocalBounds)
	{
		SkinnedLocalCenter = (LocalMin + LocalMax) * 0.5f;
		SkinnedLocalExtent = (LocalMax - LocalMin) * 0.5f;
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

bool FSkeletalMeshObjectCPU::GetSkinnedLocalBounds(FVector& OutCenter, FVector& OutExtent) const
{
	if (!bHasSkinnedLocalBounds)
	{
		return false;
	}

	OutCenter = SkinnedLocalCenter;
	OutExtent = SkinnedLocalExtent;
	return true;
}

void FSkeletalMeshObjectCPU::ApplyMorphTargets(uint32 LODIndex, const TArray<float>* Weights, TArray<FSkeletalVertex>& InOutVertices)
{
	if (!Source || !Weights)
		return;

	const int32 MorphCount = std::min<int32>(static_cast<int32>(Source->MorphTargets.size()),static_cast<int32>(Weights->size()));

	for (int32 MorphIndex = 0; MorphIndex < MorphCount; ++MorphIndex)
	{
		const float Weight = (*Weights)[MorphIndex];
		if (std::abs(Weight) <= 1.0e-4f)
			continue;

		const FMorphTarget& Morph = Source->MorphTargets[MorphIndex];
		if (LODIndex >= Morph.LODModels.size())
			continue;

		const FMorphTargetLOD& MorphLOD = Morph.LODModels[LODIndex];
		if (MorphLOD.Shapes.empty())
			continue;

		ApplyMorphLODByWeight(MorphLOD, Weight, InOutVertices);
	}

	for (FSkeletalVertex& V : InOutVertices)
	{
		V.Normal = NormalizeOrFallback(V.Normal, FVector(0.0f, 0.0f, 1.0f));

		FVector T(V.Tangent.X, V.Tangent.Y, V.Tangent.Z);
		T = OrthogonalizeTangent(T, V.Normal);
		V.Tangent = FVector4(T.X, T.Y, T.Z, V.Tangent.W);
	}
}
