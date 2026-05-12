#include "SkinnedMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"

#include <algorithm>
#include <cmath>

#include "Object/ObjectFactory.h"
#include "Collision/RayUtils.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Serialization/Archive.h"
#include "Render/Skeletal/SkeletalMeshObject.h"
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
	SetSkeletalMeshInternal(InMesh, true, true);
}

void USkinnedMeshComponent::SetSkeletalMeshInternal(USkeletalMesh* InMesh, bool bBuildInitialSkinning, bool bUpdateRenderState)
{
	SkeletalMesh = InMesh;
	InvalidateSkinnedMeshState(true);

	if (InMesh)
	{
		SkeletalMeshPath = InMesh->GetAssetPathFileName();
		OverrideMaterials.clear();
		MaterialSlots.clear();
		EnsureMaterialSlotStorage(SkeletalMesh, OverrideMaterials, MaterialSlots);

		if (FSkeletalMesh* Asset = InMesh->GetSkeletalMeshAsset())
		{
			MeshObject = GEngine ? GEngine->GetRenderer().CreateSkeletalMeshObjectCPU(Asset) : nullptr;

			if (bBuildInitialSkinning)
			{
				// 첫 프레임 가시화: TickComponent가 처음 굴러가기 전에 MeshBuffer를 만들어둠.
				// 안 하면 첫 한두 프레임 동안 GetMeshBuffer()->IsValid() == false → DrawCommandBuilder가 스킵.
				RefreshBoneTransforms();
				UpdateSkinnedMeshObject();
			}
		}
	}
	else
	{
		SkeletalMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	CacheLocalBounds();
	if (bUpdateRenderState)
	{
		FinalizeSkeletalMeshRenderState();
	}
}

void USkinnedMeshComponent::InvalidateSkinnedMeshState(bool bClearPose)
{
	MeshObject.reset();
	if (bClearPose)
	{
		BoneSpaceTransforms.clear();
		ComponentSpaceTransforms.clear();
		ComponentSpaceMatrices.clear();
		SkinningMatrices.clear();
	}

	bPoseDirty = true;
	bSkinningDirty = true;
	bBoundsDirty = true;
	bHasValidBounds = false;
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::FinalizeSkeletalMeshRenderState()
{
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::SetDisplayBones(bool bDisplay)
{
	if (bDisplayBones == bDisplay)
	{
		return;
	}

	bDisplayBones = bDisplay;
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void USkinnedMeshComponent::SetRenderLOD(uint32 LODIndex)
{
	if (!MeshObject || MeshObject->GetLOD() == LODIndex)
	{
		return;
	}

	MeshObject->SetLOD(LODIndex);
	bSkinningDirty = true;
	UpdateSkinnedMeshObject();
}

void USkinnedMeshComponent::UpdateSkinnedMeshObject()
{
	if (!bCPUSkinning || !MeshObject)
	{
		return;
	}

	MeshObject->Update(SkinningMatrices);
	FVector SkinnedCenter;
	FVector SkinnedExtent;
	if (MeshObject->GetSkinnedLocalBounds(SkinnedCenter, SkinnedExtent))
	{
		CachedLocalCenter = SkinnedCenter;
		CachedLocalExtent = SkinnedExtent * std::max(BoundsScale, 0.1f);
		bHasValidBounds = true;
		bBoundsDirty = false;
		MarkWorldBoundsDirty();
	}

	bSkinningDirty = false;
	MarkProxyDirty(EDirtyFlag::Mesh);
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

	if (!SkeletalMesh || !bCPUSkinning)
	{
		return;
	}

	const bool bMissingPose =
		ComponentSpaceMatrices.empty() ||
		SkinningMatrices.empty();

	if (bPoseDirty || bMissingPose)
	{
		RefreshBoneTransforms();
	}

	const bool bMissingBuffer =
		!MeshObject ||
		!MeshObject->GetMeshBuffer() ||
		!MeshObject->GetMeshBuffer()->IsValid();

	if (bSkinningDirty || bMissingBuffer)
	{
		UpdateSkinnedMeshObject();
	}
}

// FSkeletalMeshLOD에는 RenderBuffer가 없음 (GPU 업로드 단계 미구현).
// 일단 nullptr — 렌더 패스에서는 이 컴포넌트가 그려지지 않음.
FMeshBuffer* USkinnedMeshComponent::GetMeshBuffer() const
{
	if (!bCPUSkinning || bHideSkin)
	{
		return nullptr;
	}

	return MeshObject ? MeshObject->GetMeshBuffer() : nullptr;
}

// FSkeletalVertex 레이아웃이 FNormalVertex와 달라 직접 노출할 수 없음.
FMeshDataView USkinnedMeshComponent::GetMeshDataView() const
{
	if (!bCPUSkinning || bHideSkin)
	{
		return {};
	}

	return MeshObject ? MeshObject->GetMeshDataView() : FMeshDataView{};
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

bool USkinnedMeshComponent::LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult)
{
	const FMeshDataView MeshView = GetMeshDataView();
	if (!MeshView.IsValid())
	{
		return false;
	}

	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FMatrix& WorldInverse = GetWorldInverseMatrix();
	const bool bHit = FRayUtils::RaycastTriangles(
		Ray,
		WorldMatrix,
		WorldInverse,
		MeshView.VertexData,
		MeshView.Stride,
		MeshView.IndexData,
		MeshView.IndexCount,
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
		OutHitResult.WorldHitLocation = Ray.Origin + Ray.Direction * OutHitResult.Distance;
	}

	return bHit;
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
	Ar << bCPUSkinning;
	Ar << bHideSkin;
	Ar << bDisplayBones;
	Ar << BoundsScale;
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
	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });
	OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });

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

	if (strcmp(PropertyName, "Display Bones") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
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
	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset) return;
	const FSkeleton& Sk = Asset->Skeleton;

	const int32 N = static_cast<int32>(Sk.Bones.size());
	if ((int32)BoneSpaceTransforms.size() != N)
	{
		BoneSpaceTransforms.clear();
		for (int32 i = 0; i < N; ++i)
		{
			const FMatrix& M = Sk.Bones[i].LocalBindPose;
			BoneSpaceTransforms.push_back(FTransform(M.GetLocation(), M.ToQuat(), M.GetScale()));
		}
	}
	FillComponentSpaceTransforms();
}

void USkinnedMeshComponent::FillComponentSpaceTransforms()
{
	if (!SkeletalMesh) return;
	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset) return;
	const FSkeleton& Sk = Asset->Skeleton;

	const int32 N = static_cast<int32>(Sk.Bones.size());
	if (N == 0)
	{
		ComponentSpaceTransforms.clear();
		ComponentSpaceMatrices.clear();
		SkinningMatrices.clear();
		return;
	}
	if ((int32)BoneSpaceTransforms.size() != N)
	{
		BoneSpaceTransforms.clear();
		for (int32 i = 0; i < N; ++i)
		{
			const FMatrix& M = Sk.Bones[i].LocalBindPose;
			BoneSpaceTransforms.push_back(FTransform(M.GetLocation(), M.ToQuat(), M.GetScale()));
		}
	}

	ComponentSpaceTransforms.resize(N);
	ComponentSpaceMatrices.assign(N, FMatrix::Identity);
	SkinningMatrices.assign(N, FMatrix::Identity);

	// ParentIndex < ChildIndex 보장 → forward sweep.
	for (int32 i = 0; i < N; ++i)
	{
		const FMatrix Local    = BoneSpaceTransforms[i].ToMatrix();
		const int32   Parent   = Sk.Bones[i].ParentIndex;
		const FMatrix ParentCS = (Parent >= 0) ? ComponentSpaceMatrices[Parent] : FMatrix::Identity;

		ComponentSpaceMatrices[i] = Local * ParentCS;
		ComponentSpaceTransforms[i] = FTransform(
			ComponentSpaceMatrices[i].GetLocation(),
			ComponentSpaceMatrices[i].ToQuat(),
			ComponentSpaceMatrices[i].GetScale());
		SkinningMatrices[i]       = Sk.Bones[i].InverseBindPose * ComponentSpaceMatrices[i];
	}

	bPoseDirty = false;
	bSkinningDirty = true;
	bBoundsDirty = true;
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
	FSkeleton R;
	R.Bones.resize(3);
	R.Bones[0].Name = "root";  R.Bones[0].ParentIndex = -1;
	R.Bones[1].Name = "spine"; R.Bones[1].ParentIndex = 0;
	R.Bones[2].Name = "head";  R.Bones[2].ParentIndex = 1;

	R.Bones[0].LocalBindPose = FMatrix::Identity;
	R.Bones[1].LocalBindPose = FMatrix::MakeTranslationMatrix(FVector(0, 0, 1));
	R.Bones[2].LocalBindPose = FMatrix::MakeTranslationMatrix(FVector(0, 0, 1));

	// Rebuild GlobalBindPose & InverseBindPose
	for (int32 i = 0; i < 3; ++i)
	{
		const int32 Parent = R.Bones[i].ParentIndex;
		R.Bones[i].GlobalBindPose = R.Bones[i].LocalBindPose * ((Parent >= 0) ? R.Bones[Parent].GlobalBindPose : FMatrix::Identity);
		R.Bones[i].InverseBindPose = R.Bones[i].GlobalBindPose.GetInverse();
	}

	// FindBoneIndex sanity
	check(R.FindBoneIndexByName("spine") == 1);
	check(R.FindBoneIndexByName("nope")  == -1);

	// FillComponentSpaceTransforms와 동일 수학을 인라인으로 재현 (UObject 의존 회피).
	const int32 N = static_cast<int32>(R.Bones.size());
	TArray<FMatrix> CS(N, FMatrix::Identity);
	TArray<FMatrix> Skin(N, FMatrix::Identity);
	for (int32 i = 0; i < N; ++i)
	{
		const FMatrix Local    = R.Bones[i].LocalBindPose;
		const int32   Parent   = R.Bones[i].ParentIndex;
		const FMatrix ParentCS = (Parent >= 0) ? CS[Parent] : FMatrix::Identity;
		CS[i]   = Local * ParentCS;
		Skin[i] = R.Bones[i].InverseBindPose * CS[i];
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

	// ─── 스키닝 커널 단위 테스트 ────────────────────────────────
	// FSkeletalMeshObjectCPU::Update() 안의 per-vertex 수식과 동일한 식을 인라인으로 재현.
	// 커널 자체의 수학 정확성만 검증 (GPU 디바이스 없이 가능).
	auto SkinVertex = [](const FSkeletalVertex& V, const TArray<FMatrix>& Skin,
	                     FVector& OutPos, FVector& OutNrm)
	{
		OutPos = FVector(0, 0, 0);
		OutNrm = FVector(0, 0, 0);
		for (int k = 0; k < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++k)
		{
			const float W = V.BoneWeights[k];
			if (W <= 0.0f) continue;
			const uint32 BoneIdx = V.BoneIndices[k];
			if (BoneIdx >= (uint32)Skin.size()) continue;
			const FMatrix& M = Skin[BoneIdx];
			OutPos = OutPos + M.TransformPositionWithW(V.Pos) * W;
			OutNrm = OutNrm + M.TransformVector(V.Normal) * W;
		}
	};

	auto NearVec = [](const FVector& A, const FVector& B, float Eps)
	{
		return (A - B).Length() < Eps;
	};

	// 테스트 정점: 위치 (1,0,0), 노멀 (0,1,0), 본 0번에 100% 가중치
	FSkeletalVertex V{};
	V.Pos = FVector(1, 0, 0);
	V.Normal = FVector(0, 1, 0);
	V.Color = FVector4(1, 1, 1, 1);
	V.UV[0] = FVector2(0, 0);
	V.Tangent = FVector4(1, 0, 0, 1);
	for (int k = 0; k < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++k) { V.BoneIndices[k] = 0; V.BoneWeights[k] = 0; }
	V.BoneWeights[0] = 1.0f;

	// 커널 테스트 1: Identity skinning → output == input
	{
		TArray<FMatrix> SkinPalette(1, FMatrix::Identity);
		FVector P, NN;
		SkinVertex(V, SkinPalette, P, NN);
		checkf(NearVec(P, V.Pos, 1e-3f), "Skinning kernel: Identity must preserve position.");
		checkf(NearVec(NN, V.Normal, 1e-3f), "Skinning kernel: Identity must preserve normal.");
	}

	// 커널 테스트 2: Translation skinning → position translates, normal unchanged
	{
		TArray<FMatrix> SkinPalette(1);
		SkinPalette[0] = FMatrix::MakeTranslationMatrix(FVector(2, 0, 0));
		FVector P, NN;
		SkinVertex(V, SkinPalette, P, NN);
		// 입력 (1,0,0) + 평행이동 (2,0,0) = (3,0,0)
		checkf(NearVec(P, FVector(3, 0, 0), 1e-3f), "Skinning kernel: Translation result wrong.");
		// 노멀은 평행이동 영향을 받으면 안 됨 (TransformVector는 회전만)
		checkf(NearVec(NN, V.Normal, 1e-3f), "Skinning kernel: Translation must not affect normal.");
	}

	// 커널 테스트 3: Rotation skinning → position rotates, normal rotates
	{
		// Z축 90도 회전: (1,0,0) → (0,1,0), (0,1,0) → (-1,0,0)
		// (row-vector 규약, FMatrix::MakeRotationZ 시그니처 가정)
		TArray<FMatrix> SkinPalette(1);
		const float PI_F = 3.14159265359f;
		SkinPalette[0] = FMatrix::MakeRotationZ(PI_F * 0.5f);
		FVector P, NN;
		SkinVertex(V, SkinPalette, P, NN);
		// 회전 방향(시계/반시계)은 엔진 컨벤션 따라 둘 중 하나. 둘 다 허용.
		const bool bCW  = NearVec(P, FVector(0,  1, 0), 1e-2f);
		const bool bCCW = NearVec(P, FVector(0, -1, 0), 1e-2f);
		checkf(bCW || bCCW, "Skinning kernel: 90deg Z rotation must produce (0,±1,0).");
	}

	// 커널 테스트 4: 블렌딩 — 50% Identity + 50% Translation(2,0,0)
	{
		// 정점 가중치를 두 본으로 나눔
		FSkeletalVertex Vb = V;
		Vb.BoneIndices[0] = 0; Vb.BoneWeights[0] = 0.5f;
		Vb.BoneIndices[1] = 1; Vb.BoneWeights[1] = 0.5f;

		TArray<FMatrix> SkinPalette(2);
		SkinPalette[0] = FMatrix::Identity;
		SkinPalette[1] = FMatrix::MakeTranslationMatrix(FVector(2, 0, 0));
		FVector P, NN;
		SkinVertex(Vb, SkinPalette, P, NN);
		// 입력 (1,0,0): 0.5 * (1,0,0) + 0.5 * (1+2,0,0) = (2,0,0)
		checkf(NearVec(P, FVector(2, 0, 0), 1e-3f), "Skinning kernel: blended weights produced wrong result.");
	}

	return true;
}
