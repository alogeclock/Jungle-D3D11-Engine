#include "SkinnedMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"

#include <algorithm>
#include <cmath>

#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/Skeleton.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Serialization/Archive.h"
#include "Render/Skeletal/SkeletalMeshObject.h"
#include <Render/Skeletal/SkeletalMeshObjectCPU.h>
#include "Render/Device/D3DDevice.h"
IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

// FSkeletalMeshObject 포함 후에 디스트럭터 정의 — unique_ptr<>가 complete type을 요구.
USkinnedMeshComponent::~USkinnedMeshComponent() = default;

namespace
{
	int32 GetRequiredMaterialSlotCount(const USkeletalMesh* SkeletalMesh)
	{
		if (!SkeletalMesh)
		{
			return 0;
		}

		const TArray<FStaticMaterial>& DefaultMaterials = SkeletalMesh->GetSkeletalMaterials();
		if (!DefaultMaterials.empty())
		{
			return static_cast<int32>(DefaultMaterials.size());
		}

		const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
		if (MeshAsset && !MeshAsset->LODModels.empty())
		{
			const FSkeletalMeshLOD& LOD0 = MeshAsset->LODModels[0];
			if (!LOD0.Sections.empty() || !LOD0.Indices.empty())
			{
				return 1;
			}
		}

		return 0;
	}

	void EnsureMaterialSlotStorage(USkeletalMesh* SkeletalMesh, TArray<UMaterial*>& OverrideMaterials, TArray<FMaterialSlot>& MaterialSlots)
	{
		const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(SkeletalMesh);
		if (RequiredSlotCount <= 0)
		{
			return;
		}

		const int32 PreviousOverrideCount = static_cast<int32>(OverrideMaterials.size());
		const int32 PreviousSlotCount = static_cast<int32>(MaterialSlots.size());
		if (PreviousOverrideCount >= RequiredSlotCount && PreviousSlotCount >= RequiredSlotCount)
		{
			return;
		}

		const TArray<FStaticMaterial>& DefaultMaterials = SkeletalMesh ? SkeletalMesh->GetSkeletalMaterials() : TArray<FStaticMaterial>{};

		OverrideMaterials.resize(RequiredSlotCount, nullptr);
		MaterialSlots.resize(RequiredSlotCount);

		for (int32 i = 0; i < RequiredSlotCount; ++i)
		{
			if (i >= PreviousOverrideCount)
			{
				if (i < static_cast<int32>(DefaultMaterials.size()))
				{
					OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;
				}
				else
				{
					OverrideMaterials[i] = FMaterialManager::Get().GetOrCreateMaterial("None");
				}
			}

			if (i >= PreviousSlotCount || MaterialSlots[i].Path.empty())
			{
				MaterialSlots[i].Path = OverrideMaterials[i]
					? OverrideMaterials[i]->GetAssetPathFileName()
					: "None";
			}
		}
	}
}

FPrimitiveSceneProxy* USkinnedMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	SkeletalMesh = InMesh;
	MeshObject.reset();
	if (InMesh)
	{
		SkeletalMeshPath = InMesh->GetAssetPathFileName();
		OverrideMaterials.clear();
		MaterialSlots.clear();
		EnsureMaterialSlotStorage(SkeletalMesh, OverrideMaterials, MaterialSlots);

		if (FSkeletalMesh* Asset = InMesh->GetSkeletalMeshAsset())
		{
			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			MeshObject = std::make_unique<FSkeletalMeshObjectCPU>(Asset, Device);
		}
	}
	else
	{
		SkeletalMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::CacheLocalBounds()
{
	bHasValidBounds = false;
	if (!SkeletalMesh) return;

	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->LODModels.empty()) return;

	FSkeletalMeshLOD& LOD0 = Asset->LODModels[0];
	if (!LOD0.bBoundsValid)
	{
		LOD0.CacheBounds();
	}

	CachedLocalCenter = LOD0.BoundsCenter;
	CachedLocalExtent = LOD0.BoundsExtent;
	bHasValidBounds = LOD0.bBoundsValid;
}

void USkinnedMeshComponent::EnsureMaterialSlotsForEditing()
{
	EnsureMaterialSlotStorage(SkeletalMesh, OverrideMaterials, MaterialSlots);
}

void USkinnedMeshComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	if (ElementIndex < 0)
	{
		return;
	}

	const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(SkeletalMesh);
	if (ElementIndex >= static_cast<int32>(OverrideMaterials.size()) && ElementIndex < RequiredSlotCount)
	{
		const int32 NewSlotCount = std::max(RequiredSlotCount, ElementIndex + 1);
		OverrideMaterials.resize(NewSlotCount, nullptr);
		MaterialSlots.resize(NewSlotCount);

		for (int32 SlotIndex = 0; SlotIndex < NewSlotCount; ++SlotIndex)
		{
			if (MaterialSlots[SlotIndex].Path.empty())
			{
				MaterialSlots[SlotIndex].Path = "None";
			}
		}
	}

	if (ElementIndex < static_cast<int32>(OverrideMaterials.size()))
	{
		OverrideMaterials[ElementIndex] = InMaterial;

		if (ElementIndex < static_cast<int32>(MaterialSlots.size()))
		{
			MaterialSlots[ElementIndex].Path = InMaterial
				? InMaterial->GetAssetPathFileName()
				: "None";
		}

		MarkProxyDirty(EDirtyFlag::Material);
	}
}

UMaterial* USkinnedMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

FMaterialSlot* USkinnedMeshComponent::GetMaterialSlot(int32 ElementIndex)
{
	EnsureMaterialSlotsForEditing();
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

const FMaterialSlot* USkinnedMeshComponent::GetMaterialSlot(int32 ElementIndex) const
{
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

void USkinnedMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 본 행렬 재계산 (
	RefreshBoneTransforms();

	// 결과를 MeshObject로 전달 -> CPU 스키닝 + VB 재생성
	if (MeshObject)
	{
		MeshObject->Update(SkinningMatrices);
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

// FSkeletalMeshLOD에는 RenderBuffer가 없음 (GPU 업로드 단계 미구현).
// 일단 nullptr — 렌더 패스에서는 이 컴포넌트가 그려지지 않음.
FMeshBuffer* USkinnedMeshComponent::GetMeshBuffer() const
{
	return MeshObject ? MeshObject->GetMeshBuffer() : nullptr;
}

// FSkeletalVertex 레이아웃이 FNormalVertex와 달라 직접 노출할 수 없음.
FMeshDataView USkinnedMeshComponent::GetMeshDataView() const
{
	return {};
}

void USkinnedMeshComponent::UpdateWorldAABB() const
{
	if (!bHasValidBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector WorldCenter = CachedWorldMatrix.TransformPositionWithW(CachedLocalCenter);

	float Ex = std::abs(CachedWorldMatrix.M[0][0]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][0]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][0]) * CachedLocalExtent.Z;
	float Ey = std::abs(CachedWorldMatrix.M[0][1]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][1]) * CachedLocalExtent.Z;
	float Ez = std::abs(CachedWorldMatrix.M[0][2]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][2]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

// 스켈레탈 메시는 매 프레임 변형되므로 bind-pose BVH는 정확하지 않음.
// 현재는 picking 미지원.
bool USkinnedMeshComponent::LineTraceComponent(const FRay& /*Ray*/, FRayHitResult& /*OutHitResult*/)
{
	return false;
}

static FArchive& operator<<(FArchive& Ar, FMaterialSlot& Slot)
{
	Ar << Slot.Path;
	return Ar;
}

void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << SkeletalMeshPath;
	Ar << MaterialSlots;
}

void USkinnedMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	// USkeletalMesh 전용 로더는 후속 단계에서 연결.
	// SkeletalMeshManager 등을 통해 SkeletalMeshPath로 다시 로드하는 흐름이 들어갈 자리.

	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();

	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Skeletal Mesh", EPropertyType::StaticMeshRef, &SkeletalMeshPath });

	for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(i);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[i];
		OutProps.push_back(Desc);
	}
}

void USkinnedMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		if (SkeletalMeshPath.empty() || SkeletalMeshPath == "None")
		{
			SkeletalMesh = nullptr;
		}
		// USkeletalMesh 로더 연결은 후속 단계.
		CacheLocalBounds();
		MarkWorldBoundsDirty();
	}

	if (strncmp(PropertyName, "Element ", 8) == 0)
	{
		int32 Index = atoi(&PropertyName[8]);

		if (Index >= 0 && Index < (int32)MaterialSlots.size())
		{
			FString NewMatPath = MaterialSlots[Index].Path;

			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(NewMatPath);
				if (LoadedMat)
				{
					SetMaterial(Index, LoadedMat);
				}
			}
		}
	}
}

// ============================================================
// Bone pose pipeline
// ============================================================

void USkinnedMeshComponent::RefreshBoneTransforms()
{
	// base는 fallback으로 RefPose만 채운다.
	// 진짜 애니메이션은 USkeletalMeshComponent가 override해서 채움.
	if (!SkeletalMesh) return;
	USkeleton* Sk = SkeletalMesh->GetSkeleton();
	if (!Sk) return;

	const FReferenceSkeleton& Ref = Sk->GetReferenceSkeleton();
	if ((int32)BoneSpaceTransforms.size() != Ref.GetNum())
	{
		BoneSpaceTransforms = Ref.RefBonePose;
	}
	FillComponentSpaceTransforms();
}

void USkinnedMeshComponent::FillComponentSpaceTransforms()
{
	if (!SkeletalMesh) return;
	USkeleton* Sk = SkeletalMesh->GetSkeleton();
	if (!Sk) return;
	const FReferenceSkeleton& Ref = Sk->GetReferenceSkeleton();

	const int32 N = Ref.GetNum();
	if (N == 0)
	{
		ComponentSpaceMatrices.clear();
		SkinningMatrices.clear();
		return;
	}
	if ((int32)BoneSpaceTransforms.size() != N)
	{
		BoneSpaceTransforms = Ref.RefBonePose;
	}

	ComponentSpaceMatrices.assign(N, FMatrix::Identity);
	SkinningMatrices.assign(N, FMatrix::Identity);

	// ParentIndex < ChildIndex 보장 → forward sweep.
	// 곱 순서는 Skeleton::RebuildRefBasesInvMatrix와 동일해야 한다 (Local * Parent).
	for (int32 i = 0; i < N; ++i)
	{
		const FMatrix Local    = BoneSpaceTransforms[i].ToMatrix();
		const int32   Parent   = Ref.Bones[i].ParentIndex;
		const FMatrix ParentCS = (Parent >= 0) ? ComponentSpaceMatrices[Parent] : FMatrix::Identity;

		ComponentSpaceMatrices[i] = Local * ParentCS;
		SkinningMatrices[i]       = Ref.RefBasesInvMatrix[i] * ComponentSpaceMatrices[i];
	}
}

// ============================================================
// SelfTest — invariant 검증 (RefPose ⇒ SkinningMatrix == Identity + RTTI 체인)
// ============================================================

bool USkinnedMeshComponent::SelfTest()
{
	auto IsNearIdentity = [](const FMatrix& M, float Eps) -> bool
	{
		for (int r = 0; r < 4; ++r)
		{
			for (int c = 0; c < 4; ++c)
			{
				const float Expected = (r == c) ? 1.0f : 0.0f;
				if (std::abs(M.M[r][c] - Expected) > Eps) return false;
			}
		}
		return true;
	};

	// 본 3개: root → spine(+Z 1) → head(+Z 1)
	FReferenceSkeleton R;
	R.Allocate(3);
	R.Bones[0] = { FName("root"),  -1 };
	R.Bones[1] = { FName("spine"),  0 };
	R.Bones[2] = { FName("head"),   1 };
	R.RefBonePose[0] = FTransform();
	R.RefBonePose[1] = FTransform(FVector(0, 0, 1), FQuat::Identity, FVector(1, 1, 1));
	R.RefBonePose[2] = FTransform(FVector(0, 0, 1), FQuat::Identity, FVector(1, 1, 1));
	R.RebuildRefBasesInvMatrix();

	// FindBoneIndex sanity
	check(R.FindBoneIndex(FName("spine")) == 1);
	check(R.FindBoneIndex(FName("nope"))  == -1);

	// FillComponentSpaceTransforms와 동일 수학을 인라인으로 재현 (UObject 의존 회피).
	const int32 N = R.GetNum();
	TArray<FMatrix> CS(N, FMatrix::Identity);
	TArray<FMatrix> Skin(N, FMatrix::Identity);
	for (int32 i = 0; i < N; ++i)
	{
		const FMatrix Local    = R.RefBonePose[i].ToMatrix();
		const int32   Parent   = R.Bones[i].ParentIndex;
		const FMatrix ParentCS = (Parent >= 0) ? CS[Parent] : FMatrix::Identity;
		CS[i]   = Local * ParentCS;
		Skin[i] = R.RefBasesInvMatrix[i] * CS[i];
	}

	// invariant 1: RefPose ⇒ 모든 SkinningMatrix가 Identity
	for (int32 i = 0; i < N; ++i)
	{
		checkf(IsNearIdentity(Skin[i], 1e-3f),
		       "Skinning matrix at RefPose must be Identity. Matrix multiply order mismatch between Skeleton::RebuildRefBasesInvMatrix and USkinnedMeshComponent::FillComponentSpaceTransforms.");
	}

	// invariant 2: head의 component-space translation == (0, 0, 2)
	const FVector HeadPos = CS[2].TransformPositionWithW(FVector(0, 0, 0));
	checkf(std::abs(HeadPos.Z - 2.0f) < 1e-3f, "Forward sweep produced incorrect component-space translation.");

	// invariant 3: RTTI 체인
	check(USkeletalMeshComponent::StaticClass()->IsA(USkinnedMeshComponent::StaticClass()));
	check(USkinnedMeshComponent::StaticClass()->IsA(UMeshComponent::StaticClass()));

	return true;
}
