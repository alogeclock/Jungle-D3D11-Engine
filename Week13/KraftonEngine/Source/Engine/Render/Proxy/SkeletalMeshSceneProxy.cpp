#include "SkeletalMeshSceneProxy.h"
#include "Component/SkeletalMeshComponent.h"
#include "Debug/DebugDrawQueue.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletonAsset.h"
#include "Physics/Assets/PhysicsAsset.h"
#include "Physics/Assets/PhysicsBodySetup.h"
#include "Physics/Assets/PhysicsShapeSetup.h"
#include "Render/Command/DrawCommand.h"
#include "Runtime/Engine.h"
#include "Profiling/Timer.h"
#include "Profiling/Stats.h"
#include <Profiling/SkinningStats.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr int32 CollisionSegments = 24;
	constexpr float CollisionLineDuration = 0.0f;
	const FColor CollisionColor = FColor(0, 255, 0);
	const FVector PhysicsCapsuleAxis = FVector::ForwardVector;

	int32 FindBoneIndex(const FSkeletonAsset* SkeletonAsset, const FName& BoneName)
	{
		if (!SkeletonAsset)
		{
			return INDEX_NONE;
		}

		const FString Target = BoneName.ToString();
		for (int32 Index = 0; Index < static_cast<int32>(SkeletonAsset->Bones.size()); ++Index)
		{
			if (SkeletonAsset->Bones[Index].Name == Target)
			{
				return Index;
			}
		}

		return INDEX_NONE;
	}

	FMatrix GetBoneWorld(
		const USkeletalMeshComponent* Comp,
		const TArray<FMatrix>& BoneGlobals,
		int32 BoneIndex)
	{
		if (!Comp)
		{
			return FMatrix::Identity;
		}

		const FMatrix& MeshWorld = Comp->GetWorldMatrix();
		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(BoneGlobals.size()))
		{
			return BoneGlobals[BoneIndex] * MeshWorld;
		}

		return MeshWorld;
	}

	void AddLine(FDebugDrawQueue& Queue, const FMatrix& Xf, const FVector& A, const FVector& B)
	{
		Queue.AddLine(
			Xf.TransformPositionWithW(A),
			Xf.TransformPositionWithW(B),
			CollisionColor,
			CollisionLineDuration);
	}

	void AddCircle(
		FDebugDrawQueue& Queue,
		const FMatrix& Xf,
		const FVector& Center,
		const FVector& AxisA,
		const FVector& AxisB,
		float Radius)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		const float Step = 2.0f * Pi / static_cast<float>(CollisionSegments);
		FVector Prev = Center + AxisA * Radius;
		for (int32 Index = 1; Index <= CollisionSegments; ++Index)
		{
			const float Angle = Step * static_cast<float>(Index);
			const FVector Next = Center + (AxisA * std::cos(Angle) + AxisB * std::sin(Angle)) * Radius;
			AddLine(Queue, Xf, Prev, Next);
			Prev = Next;
		}
	}

	void AddArc(FDebugDrawQueue& Queue, const FMatrix& Xf, const FVector& Center, const FVector& AxisA, const FVector& AxisB, float Radius, float StartAngle)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		constexpr int32 ArcSegments = 12;
		const float Step = Pi / static_cast<float>(ArcSegments);
		FVector Prev = Center + (AxisA * std::cos(StartAngle) + AxisB * std::sin(StartAngle)) * Radius;
		for (int32 Index = 1; Index <= ArcSegments; ++Index)
		{
			const float Angle = StartAngle + Step * static_cast<float>(Index);
			const FVector Next = Center + (AxisA * std::cos(Angle) + AxisB * std::sin(Angle)) * Radius;
			AddLine(Queue, Xf, Prev, Next);
			Prev = Next;
		}
	}

	void AddBox(FDebugDrawQueue& Queue, const FMatrix& Xf, const FVector& HalfExtent)
	{
		const FVector Ext(
			std::abs(HalfExtent.X),
			std::abs(HalfExtent.Y),
			std::abs(HalfExtent.Z));

		const FVector C[8] = {
			FVector(-Ext.X, -Ext.Y, -Ext.Z),
			FVector( Ext.X, -Ext.Y, -Ext.Z),
			FVector( Ext.X,  Ext.Y, -Ext.Z),
			FVector(-Ext.X,  Ext.Y, -Ext.Z),
			FVector(-Ext.X, -Ext.Y,  Ext.Z),
			FVector( Ext.X, -Ext.Y,  Ext.Z),
			FVector( Ext.X,  Ext.Y,  Ext.Z),
			FVector(-Ext.X,  Ext.Y,  Ext.Z),
		};

		AddLine(Queue, Xf, C[0], C[1]);
		AddLine(Queue, Xf, C[1], C[2]);
		AddLine(Queue, Xf, C[2], C[3]);
		AddLine(Queue, Xf, C[3], C[0]);
		AddLine(Queue, Xf, C[4], C[5]);
		AddLine(Queue, Xf, C[5], C[6]);
		AddLine(Queue, Xf, C[6], C[7]);
		AddLine(Queue, Xf, C[7], C[4]);
		AddLine(Queue, Xf, C[0], C[4]);
		AddLine(Queue, Xf, C[1], C[5]);
		AddLine(Queue, Xf, C[2], C[6]);
		AddLine(Queue, Xf, C[3], C[7]);
	}

	void AddSphere(FDebugDrawQueue& Queue, const FMatrix& Xf, float Radius)
	{
		const float R = std::max(0.001f, std::abs(Radius));
		AddCircle(Queue, Xf, FVector::ZeroVector, FVector::ForwardVector, FVector::RightVector, R);
		AddCircle(Queue, Xf, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector, R);
		AddCircle(Queue, Xf, FVector::ZeroVector, FVector::RightVector, FVector::UpVector, R);
	}

	void AddCapsule(FDebugDrawQueue& Queue, const FMatrix& Xf, float Radius, float Length)
	{
		const float R = std::max(0.001f, std::abs(Radius));
		const float HalfLen = std::max(0.0f, std::abs(Length) * 0.5f);
		const FVector Top = PhysicsCapsuleAxis * HalfLen;
		const FVector Bottom = PhysicsCapsuleAxis * -HalfLen;

		AddCircle(Queue, Xf, Top, FVector::RightVector, FVector::UpVector, R);
		AddCircle(Queue, Xf, Bottom, FVector::RightVector, FVector::UpVector, R);

		AddLine(Queue, Xf, Bottom + FVector::RightVector * R, Top + FVector::RightVector * R);
		AddLine(Queue, Xf, Bottom - FVector::RightVector * R, Top - FVector::RightVector * R);
		AddLine(Queue, Xf, Bottom + FVector::UpVector * R, Top + FVector::UpVector * R);
		AddLine(Queue, Xf, Bottom - FVector::UpVector * R, Top - FVector::UpVector * R);

		AddArc(Queue, Xf, Top, PhysicsCapsuleAxis, FVector::RightVector, R, 0.0f);
		AddArc(Queue, Xf, Top, PhysicsCapsuleAxis, FVector::LeftVector, R, 0.0f);
		AddArc(Queue, Xf, Top, PhysicsCapsuleAxis, FVector::UpVector, R, 0.0f);
		AddArc(Queue, Xf, Top, PhysicsCapsuleAxis, FVector::DownVector, R, 0.0f);

		AddArc(Queue, Xf, Bottom, PhysicsCapsuleAxis * -1.0f, FVector::RightVector, R, 0.0f);
		AddArc(Queue, Xf, Bottom, PhysicsCapsuleAxis * -1.0f, FVector::LeftVector, R, 0.0f);
		AddArc(Queue, Xf, Bottom, PhysicsCapsuleAxis * -1.0f, FVector::UpVector, R, 0.0f);
		AddArc(Queue, Xf, Bottom, PhysicsCapsuleAxis * -1.0f, FVector::DownVector, R, 0.0f);
	}

	void AddConvexBounds(FDebugDrawQueue& Queue, const FMatrix& Xf, const FPhysicsConvexShapeSetup& Shape)
	{
		if (Shape.VertexData.empty())
		{
			AddBox(Queue, Xf, FVector(5.0f, 5.0f, 5.0f));
			return;
		}

		FVector Min = Shape.VertexData[0];
		FVector Max = Shape.VertexData[0];
		for (const FVector& Vertex : Shape.VertexData)
		{
			Min.X = std::min(Min.X, Vertex.X);
			Min.Y = std::min(Min.Y, Vertex.Y);
			Min.Z = std::min(Min.Z, Vertex.Z);
			Max.X = std::max(Max.X, Vertex.X);
			Max.Y = std::max(Max.Y, Vertex.Y);
			Max.Z = std::max(Max.Z, Vertex.Z);
		}

		const FVector Center = (Min + Max) * 0.5f;
		const FVector Extent = (Max - Min) * 0.5f;
		AddBox(Queue, FMatrix::MakeTranslationMatrix(Center) * Xf, Extent);
	}
}

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::SkeletalMesh;
}

FSkeletalMeshSceneProxy::~FSkeletalMeshSceneProxy()
{
	DynamicVertexBuffer.Release();
	SkinMatrixBuffer.Release();
	SkeletalRenderCB.Release();
}   

USkeletalMeshComponent* FSkeletalMeshSceneProxy::GetSkeletalMeshComponent() const
{
	return static_cast<USkeletalMeshComponent*>(GetOwner());
}

void FSkeletalMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
};

void FSkeletalMeshSceneProxy::UpdateMesh()
{
	MeshBuffer = GetOwner()->GetMeshBuffer();
	RebuildSectionDraws();

	CachedDynamicVertexCount = 0;
	UploadedSkinnedRevision = 0;
	bDynamicBufferNeedsCreate = true;

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = SMC ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (Asset)
	{
		CachedDynamicVertexCount = static_cast<uint32>(Asset->Vertices.size());
	}
}

const char* FSkeletalMeshSceneProxy::GetVertexShaderEntryName() const
{
	return "VS_Skeletal";
}

bool FSkeletalMeshSceneProxy::WantsGpuSkinning(const FPrimitiveDrawOptions& Options) const
{
	return Options.SkinningMode == ESkinningMode::GPU;
}

bool FSkeletalMeshSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	SCOPE_STAT_CAT("Prepare Buffer(CPU)", "Skinning");

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!SMC) return false;

	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || !Asset->RenderBuffer || !Asset->RenderBuffer->IsValid()) return false;

	SMC->EnsureCPUSkinnedVertices();

	const TArray<FVertexPNCTBW>& SkinnedVertices = SMC->GetSkinnedVertices();
	const uint32 VertexCount = static_cast<uint32>(SkinnedVertices.size());
	if (VertexCount == 0) return false;

	SkinningStats::RecordCPUSkinnedMeshVertices(Mesh, VertexCount, sizeof(FVertexPNCTBW));

	if (bDynamicBufferNeedsCreate || !DynamicVertexBuffer.GetBuffer())
	{
		DynamicVertexBuffer.Create(Device, CachedDynamicVertexCount ? CachedDynamicVertexCount : VertexCount, sizeof(FVertexPNCTBW));
		bDynamicBufferNeedsCreate = false;
	}

	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);

	const uint64 CurrentRevision = SMC->GetSkinnedRevision();
	if (UploadedSkinnedRevision != CurrentRevision)
	{
		if (!DynamicVertexBuffer.Update(Context, SkinnedVertices.data(), VertexCount))
		{
			return false;
		}
		UploadedSkinnedRevision = CurrentRevision;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = Asset->RenderBuffer->GetIndexBuffer().GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

bool FSkeletalMeshSceneProxy::PrepareGpuSkinningDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	SCOPE_STAT_CAT("Prepare Buffer(GPU)", "Skinning");

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!SMC) return false;

	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || !Asset->RenderBuffer || !Asset->RenderBuffer->IsValid()) return false;

	OutBuffer = {};
	OutBuffer.VB = Asset->RenderBuffer->GetVertexBuffer().GetBuffer();
	OutBuffer.VBStride = Asset->RenderBuffer->GetVertexBuffer().GetStride();
	OutBuffer.IB = Asset->RenderBuffer->GetIndexBuffer().GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

bool FSkeletalMeshSceneProxy::PrepareDrawCommandBindings(ID3D11Device* Device, ID3D11DeviceContext* Context,
	const FPrimitiveDrawOptions& Options, FDrawCommand& OutCommand) const
{
	if (!Device || !Context)
	{
		return false;
	}

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!SMC)
	{
		return false;
	}

	if (!SkeletalRenderCB.GetBuffer())
	{
		SkeletalRenderCB.Create(Device, sizeof(FSkeletalRenderConstants), "SkeletalRenderCB");
	}

	FSkeletalRenderConstants Constants = {};
	Constants.SkinningMode = static_cast<uint32>(Options.SkinningMode);
	Constants.HeatmapMode = Options.bBoneWeightHeatmap ? 1u : 0u;
	Constants.SelectedBoneIndex = Options.BoneWeightHeatmapBoneIndex;
	SkeletalRenderCB.Update(Context, &Constants, sizeof(Constants));

	OutCommand.Skinning.SkeletalRenderCB = &SkeletalRenderCB;
	OutCommand.Skinning.SkinMatrixSRV = nullptr;
	OutCommand.Skinning.bEnabled = true;

	if (Options.SkinningMode == ESkinningMode::GPU)
	{
		SCOPE_STAT_CAT("Prepare Buffer(GPU)", "Skinning");
		const TArray<FMatrix>& SkinMatrices = SMC->GetCurrentSkinMatrices();
		if (SkinMatrices.empty())
		{
			return false;
		}

		const uint32 MatrixCount = static_cast<uint32>(SkinMatrices.size());
		SkinMatrixBuffer.EnsureCapacity(Device, MatrixCount, sizeof(FMatrix));

		// Skinning Stat
		SkinningStats::RecordSkinMatrixStructuredBuffer(
			SMC,
			MatrixCount,
			SkinMatrixBuffer.GetStride()
		);

		const uint64 CurrentRevision = SMC->GetSkinMatrixRevision();
		if (UploadedSkinMatrixRevision != CurrentRevision)
		{
			if (!SkinMatrixBuffer.Update(Context, SkinMatrices.data(), MatrixCount))
			{
				return false;
			}

			UploadedSkinMatrixRevision = CurrentRevision;
		}

		OutCommand.Skinning.SkinMatrixSRV = SkinMatrixBuffer.GetSRV();
	}

	return true;
}

void FSkeletalMeshSceneProxy::CollectCollisionShapes(FDebugDrawQueue& Queue) const
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	if (!SMC)
	{
		return;
	}

	UPhysicsAsset* PhysicsAsset = SMC->GetPhysicsAsset();
	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	const FSkeletonAsset* Skeleton = Mesh ? Mesh->GetSkeletonAsset() : nullptr;
	if (!PhysicsAsset || !Skeleton)
	{
		return;
	}

	TArray<FMatrix> BoneGlobals;
	SMC->GetCurrentBoneGlobalMatrices(BoneGlobals);

	for (const UPhysicsBodySetup* Body : PhysicsAsset->GetBodySetups())
	{
		if (!Body)
		{
			continue;
		}

		const int32 BoneIndex = FindBoneIndex(Skeleton, Body->GetTargetBoneName());
		const FMatrix BoneWorld = GetBoneWorld(SMC, BoneGlobals, BoneIndex);
		const FPhysicsAggregateShapeSetup& Shapes = Body->GetShapeSetup();

		for (const FPhysicsSphereShapeSetup& Shape : Shapes.SphereShapeSetups)
		{
			AddSphere(Queue, Shape.LocalTransform.ToMatrix() * BoneWorld, Shape.Radius);
		}
		for (const FPhysicsBoxShapeSetup& Shape : Shapes.BoxShapeSetups)
		{
			AddBox(Queue, Shape.LocalTransform.ToMatrix() * BoneWorld, Shape.HalfExtent);
		}
		for (const FPhysicsCapsuleShapeSetup& Shape : Shapes.CapsuleShapeSetups)
		{
			AddCapsule(Queue, Shape.LocalTransform.ToMatrix() * BoneWorld, Shape.Radius, Shape.Length);
		}
		for (const FPhysicsConvexShapeSetup& Shape : Shapes.ConvexShapeSetups)
		{
			AddConvexBounds(Queue, Shape.LocalTransform.ToMatrix() * BoneWorld, Shape);
		}
	}
}

void FSkeletalMeshSceneProxy::RebuildSectionDraws()
{
	SectionDraws.clear();

	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();

		return;
	}

	SectionDraws.clear();

	const auto& Slots = Mesh->GetSkeletalMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();

	for (const FSkeletalMeshSection& Section : Mesh->GetSkeletalMeshAsset()->Sections)
	{
		FMeshSectionDraw Draw;
		Draw.Material = nullptr;
		Draw.FirstIndex = Section.FirstIndex;
		Draw.IndexCount = Section.IndexCount;


		int32 i = Section.MaterialIndex;
		if (i >= 0 && i < static_cast<int32>(Slots.size()))
		{
			if (i < static_cast<int32>(Overrides.size()) && Overrides[i])
				Draw.Material = Overrides[i];
			else if (Slots[i].MaterialInterface)
				Draw.Material = Slots[i].MaterialInterface;
		}

		if (!Draw.Material)
		{
			Draw.Material = FMaterialManager::Get().GetOrCreateMaterial("None");
		}

		SectionDraws.push_back(Draw);
	}
}
