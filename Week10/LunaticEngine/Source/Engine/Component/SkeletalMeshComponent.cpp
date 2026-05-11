#include "Component/SkeletalMeshComponent.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>
#include <string>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	// 스켈레탈 메시에 필요한 최소 머티리얼 슬롯(Sub Mesh) 개수를 반환한다.
    int32 GetRequiredMaterialSlotCount(const USkeletalMesh* SkeletalMesh)
    {
        if (!SkeletalMesh) return 0;
        const auto& Mats = SkeletalMesh->GetSkeletalMaterials();
        if (!Mats.empty()) return static_cast<int32>(Mats.size());

        const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
        const FSkeletalMeshLOD* LOD = MeshAsset ? MeshAsset->GetLOD(0) : nullptr;
        return (LOD && (!LOD->Sections.empty() || !LOD->Indices.empty())) ? 1 : 0;
    }

	// FMatrix에서 회전값을 정규화하여 FTransform 구조체로 추출한다.
    FTransform TransformFromMatrix(const FMatrix& Matrix)
    {
        FQuat Rotation = Matrix.ToQuat();
        Rotation.Normalize();
        return { Matrix.GetLocation(), Rotation, Matrix.GetScale() };
    }
}

// 렌더링에 사용할 Mesh Buffer를 반환한다.
FMeshBuffer* USkeletalMeshComponent::GetMeshBuffer() const
{
    return SkeletalMeshAsset ? SkeletalMeshAsset->GetMeshBuffer() : nullptr;
}

// CPU Skinning에 사용될 LOD 0의 원본 Vertex/Index 데이터 뷰를 반환한다.
FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
    if (SkeletalMeshAsset && SkeletalMeshAsset->GetSkeletalMeshAsset())
    {
        if (const FSkeletalMeshLOD* LOD = SkeletalMeshAsset->GetSkeletalMeshAsset()->GetLOD(0))
        {
            if (!LOD->Vertices.empty())
            {
                FMeshDataView View;
                View.VertexData = LOD->Vertices.data();
                View.VertexCount = static_cast<uint32>(LOD->Vertices.size());
                View.Stride = sizeof(FSkeletalVertex);
                View.IndexData = LOD->Indices.data();
                View.IndexCount = static_cast<uint32>(LOD->Indices.size());

                return View;
            }
        }
    }
    return {};
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

// 디버그 렌더링용으로 씬에 뼈대 계층 구조를 선으로 그린다.
void USkeletalMeshComponent::ContributeVisuals(FScene& Scene) const
{
    if (!bShowSkeleton || !SkeletalMeshAsset || !SkeletalMeshAsset->GetSkeletalMeshAsset()) return;

    const FSkeleton& Skeleton = SkeletalMeshAsset->GetSkeletalMeshAsset()->Skeleton;
    if (Skeleton.Bones.empty()) return;

    const FMatrix& ComponentWorld = GetWorldMatrix();

	// 현재 계산된 컴포넌트 트랜스폼 혹은 Global Bind-Pose Transform 행렬을 반환한다.
    auto GetBoneMatrix = [&](int32 Idx) {
        return (Idx >= 0 && Idx < ComponentSpaceTransforms.size()) ? ComponentSpaceTransforms[Idx].ToMatrix() : Skeleton.Bones[Idx].GlobalBindPose;
    };

    for (int32 i = 0; i < Skeleton.Bones.size(); ++i)
    {
        int32 ParentIdx = Skeleton.Bones[i].ParentIndex;
        if (!IsValidBoneIndex(ParentIdx)) continue;

        FVector ParentWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(ParentIdx).GetLocation());
        FVector BoneWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(i).GetLocation());

        bool bSelected = (i == SelectedBoneIndex || ParentIdx == SelectedBoneIndex);
        Scene.AddForegroundDebugLine(ParentWorld, BoneWorld, bSelected ? FColor::Yellow() : FColor(190, 205, 215, 255));
    }
}

// 특정 본의 로컬 공간 트랜스폼 포인터를 반환한다.
const FTransform* USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    return IsValidBoneIndex(BoneIndex) ? &BoneSpaceTransforms[BoneIndex] : nullptr;
}

// 특정 본의 컴포넌트 공간 트랜스폼 포인터를 반환한다.
const FTransform* USkeletalMeshComponent::GetBoneComponentSpaceTransform(int32 BoneIndex) const
{
    return IsValidBoneIndex(BoneIndex) ? &ComponentSpaceTransforms[BoneIndex] : nullptr;
}

void USkeletalMeshComponent::SetSelectedBoneIndex(int32 BoneIndex)
{
    SelectedBoneIndex = IsValidBoneIndex(BoneIndex) ? BoneIndex : -1;
}

// 특정 본의 로컬 공간 트랜스폼을 수정하고 하위 트랜스폼을 갱신한다.
void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform)
{
    if (!IsValidBoneIndex(BoneIndex)) return;
    if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size()) InitializeBoneTransformsFromSkeleton();

    BoneSpaceTransforms[BoneIndex] = NewTransform;
    RefreshBoneTransforms();
    MarkProxyDirty(EDirtyFlag::Mesh);
    MarkWorldBoundsDirty();
}

// 특정 본의 트랜스폼을 컴포넌트 공간 기준으로 설정한다. (부모 본의 역행렬을 곱해 Local Space로 변환)
bool USkeletalMeshComponent::SetBoneComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
    if (!IsValidBoneIndex(BoneIndex)) return false;

    const FSkeleton& Skeleton = SkeletalMeshAsset->GetSkeletalMeshAsset()->Skeleton;
    FMatrix LocalMatrix = NewTransform.ToMatrix();
    int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

    if (IsValidBoneIndex(ParentIndex))
    {
        FMatrix ParentMat = ParentIndex < ComponentSpaceTransforms.size() ? ComponentSpaceTransforms[ParentIndex].ToMatrix() : Skeleton.Bones[ParentIndex].GlobalBindPose;
        LocalMatrix = LocalMatrix * ParentMat.GetInverse();
    }

    SetBoneLocalTransform(BoneIndex, TransformFromMatrix(LocalMatrix));
    return true;
}

// 로컬 트랜스폼 배열을 기반으로 컴포넌트 공간 트랜스폼 및 스키닝 행렬을 계산한다.
void USkeletalMeshComponent::RefreshBoneTransforms()
{
    const FSkeleton* Skeleton = (SkeletalMeshAsset && SkeletalMeshAsset->GetSkeletalMeshAsset()) ? &SkeletalMeshAsset->GetSkeletalMeshAsset()->Skeleton : nullptr;
    int32 BoneCount = Skeleton ? static_cast<int32>(Skeleton->Bones.size()) : 0;

    if (BoneCount == 0)
    {
        ComponentSpaceTransforms.clear();
        SkinningMatrices.clear();
        RequiredBones.clear();
        SelectedBoneIndex = -1;
        bRequiredBonesUpdated = true;
        bPoseDirty = bSkinningDirty = false;
        return;
    }

    if (BoneSpaceTransforms.size() != BoneCount)
    {
        InitializeBoneTransformsFromSkeleton();
        return;
    }

    ComponentSpaceTransforms.resize(BoneCount);
    SkinningMatrices.resize(BoneCount);
    RequiredBones.resize(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        RequiredBones[i] = i;
        FMatrix CompMat = BoneSpaceTransforms[i].ToMatrix();
        int32 ParentIdx = Skeleton->Bones[i].ParentIndex;

        if (ParentIdx >= 0 && ParentIdx < i)
            CompMat = CompMat * ComponentSpaceTransforms[ParentIdx].ToMatrix();

        ComponentSpaceTransforms[i] = TransformFromMatrix(CompMat);
        SkinningMatrices[i] = Skeleton->Bones[i].InverseBindPose * CompMat;
    }

    SelectedBoneIndex = IsValidBoneIndex(SelectedBoneIndex) ? SelectedBoneIndex : -1;
    bRequiredBonesUpdated = true;
    bPoseDirty = bSkinningDirty = false;
    bBoundsDirty = true;
}

// 부모 클래스의 스킨드 에셋 경로와 현재 컴포넌트의 에셋 경로를 동기화한다.
void USkeletalMeshComponent::SyncSkinnedAssetPath()
{
    if (SkeletalMeshAssetPath.empty()) SkeletalMeshAssetPath = "None";
    SkinnedMeshAssetPath = SkeletalMeshAssetPath;
    SkinnedAsset = SkeletalMeshAsset;
}

// 애니메이션 포즈, 설정 변경 시 스키닝 행렬, AABB, 프록시 갱신을 위해 더티 플래그를 갱신한다.
void USkeletalMeshComponent::MarkSkeletalPoseDirty()
{
    bRequiredBonesUpdated = false;
    bPoseDirty = bSkinningDirty = bBoundsDirty = true;
    MarkProxyDirty(EDirtyFlag::Mesh);
    MarkWorldBoundsDirty();
}

// 새로운 스켈레탈 메시 에셋을 할당하고 관련된 데이터를 초기화한다.
void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMeshAsset = InSkeletalMesh;
    SkeletalMeshAssetPath = SkeletalMeshAsset ? SkeletalMeshAsset->GetAssetPathFileName() : "None";

    SyncSkinnedAssetPath();
    EnsureMaterialSlotsForEditing();
    CacheLocalBounds();
    InitializeBoneTransformsFromSkeleton();
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

// 메시에 필요한 머티리얼 슬롯 개수를 파악한 뒤, 부족한 경우 Default Material로 동적 할당한다.
void USkeletalMeshComponent::EnsureMaterialSlotsForEditing()
{
    if (SkeletalMeshAssetPath.empty()) SkeletalMeshAssetPath = "None";
    int32 ReqCount = GetRequiredMaterialSlotCount(SkeletalMeshAsset);

    if (ReqCount <= 0)
    {
        OverrideMaterials.clear();
        MaterialSlots.clear();
        return;
    }

    OverrideMaterials.resize(ReqCount, nullptr);
    MaterialSlots.resize(ReqCount);

    const auto& DefaultMats = SkeletalMeshAsset ? SkeletalMeshAsset->GetSkeletalMaterials() : TArray<FStaticMaterial>{};
    for (int32 i = 0; i < ReqCount; ++i)
    {
        if (!OverrideMaterials[i])
        {
            OverrideMaterials[i] = (i < DefaultMats.size()) ? DefaultMats[i].MaterialInterface : FMaterialManager::Get().GetOrCreateMaterial("None");
        }
        if (MaterialSlots[i].Path.empty())
        {
            MaterialSlots[i].Path = OverrideMaterials[i] ? OverrideMaterials[i]->GetAssetPathFileName() : "None";
        }
    }
}

// 보일러플레이트 직렬화 함수
void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
    if (Ar.IsSaving()) SyncSkinnedAssetPath();

    USkinnedMeshComponent::Serialize(Ar);

    Ar << SkeletalMeshAssetPath;
    Ar << BoneSpaceTransforms; // 1차 배열 직렬화 구조 그대로 반영
    Ar << bForceRefPose << bEnableSkeletonUpdate << RootBoneTranslation << bShowSkeleton << bShowBoneNames;

    if (Ar.IsLoading())
    {
        if (SkeletalMeshAssetPath.empty()) SkeletalMeshAssetPath = "None";
        if (SkeletalMeshAssetPath == "None" && !SkinnedMeshAssetPath.empty() && SkinnedMeshAssetPath != "None")
            SkeletalMeshAssetPath = SkinnedMeshAssetPath;

        SyncSkinnedAssetPath();
        CacheLocalBounds();
        BoneSpaceTransforms.empty() ? InitializeBoneTransformsFromSkeleton() : RefreshBoneTransforms();
    }
}

// 보일러플레이트 Duplicate() 이후 처리 함수
void USkeletalMeshComponent::PostDuplicate()
{
    USkinnedMeshComponent::PostDuplicate();

    if (SkeletalMeshAssetPath.empty()) SkeletalMeshAssetPath = "None";
    if (SkeletalMeshAssetPath == "None" && !SkinnedMeshAssetPath.empty() && SkinnedMeshAssetPath != "None")
        SkeletalMeshAssetPath = SkinnedMeshAssetPath;

    SyncSkinnedAssetPath();
    CacheLocalBounds();
    RefreshBoneTransforms();
}

// 프로퍼티에 노출할 멤버 변수를 결정하는 보일러플레이트 함수
void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    EnsureMaterialSlotsForEditing();
    UMeshComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshAssetPath });
    OutProps.push_back({ "CPU Skinning", EPropertyType::Bool, &bCPUSkinning });
    OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });
    OutProps.push_back({ "Hide Skin", EPropertyType::Bool, &bHideSkin });
    OutProps.push_back({ "Force Ref Pose", EPropertyType::Bool, &bForceRefPose });
    OutProps.push_back({ "Enable Skeleton Update", EPropertyType::Bool, &bEnableSkeletonUpdate });
    OutProps.push_back({ "Root Bone", EPropertyType::Vec3, &RootBoneTranslation });
    OutProps.push_back({ "Selected Bone Index", EPropertyType::Int, &SelectedBoneIndex, -1.0f, 100000.0f, 1.0f });
    OutProps.push_back({ "Show Skeleton", EPropertyType::Bool, &bShowSkeleton });
    OutProps.push_back({ "Show Bone Names", EPropertyType::Bool, &bShowBoneNames });

    for (int32 i = 0; i < MaterialSlots.size(); ++i)
    {
	    OutProps.push_back({ "Element " + std::to_string(i), EPropertyType::MaterialSlot, &MaterialSlots[i] });
    }
}

// 프로퍼티 갱신 후 처리를 결정하는 보일러플레이트 함수
void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
    USkinnedMeshComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Skeletal Mesh") == 0)
    {
        if (SkeletalMeshAssetPath.empty() || SkeletalMeshAssetPath == "None")
        {
            SkeletalMeshAsset = SkinnedAsset = nullptr;
            SkeletalMeshAssetPath = "None";
            SyncSkinnedAssetPath();
            EnsureMaterialSlotsForEditing();
            InitializeBoneTransformsFromSkeleton();
            MarkRenderStateDirty();
        }
        else SetSkeletalMesh(FSkeletalMeshManager::LoadSkeletalMesh(SkeletalMeshAssetPath));
    }
    else if (std::strcmp(PropertyName, "Force Ref Pose") == 0 || std::strcmp(PropertyName, "Enable Skeleton Update") == 0 || std::strcmp(PropertyName, "Root Bone Translation") == 0)
    {
        MarkSkeletalPoseDirty();
    }
    else if (std::strcmp(PropertyName, "Selected Bone Index") == 0 || std::strcmp(PropertyName, "Show Skeleton") == 0 || std::strcmp(PropertyName, "Show Bone Names") == 0)
    {
        SetSelectedBoneIndex(SelectedBoneIndex);
        MarkProxyDirty(EDirtyFlag::Mesh);
    }
}

// LOD 0 스켈레탈 메시에서 Local AABB를 가져와서 컴포넌트에 캐싱한다.
void USkeletalMeshComponent::CacheLocalBounds()
{
    bHasValidBounds = false;
    LocalCenter = FVector(0.0f, 0.0f, 0.0f);
    LocalExtent = FVector(0.5f, 0.5f, 0.5f);

    if (SkeletalMeshAsset && SkeletalMeshAsset->GetSkeletalMeshAsset() && SkeletalMeshAsset->GetSkeletalMeshAsset()->GetLOD(0))
    {
        FSkeletalMeshLOD* LOD = SkeletalMeshAsset->GetSkeletalMeshAsset()->GetLOD(0);
        if (!LOD->bBoundsValid) LOD->CacheBounds();

        LocalCenter = LOD->BoundsCenter;
        LocalExtent = LOD->BoundsExtent;
        bHasValidBounds = LOD->bBoundsValid;
    }
}

// Mesh Asset의 원본 정보(Local Bind Pose)를 기반으로 로컬 본 트랜스폼 배열을 초기화한다.
void USkeletalMeshComponent::InitializeBoneTransformsFromSkeleton()
{
    BoneSpaceTransforms.clear();
    ComponentSpaceTransforms.clear();
    SkinningMatrices.clear();
    RequiredBones.clear();

    const FSkeleton* Skeleton = (SkeletalMeshAsset && SkeletalMeshAsset->GetSkeletalMeshAsset()) ? &SkeletalMeshAsset->GetSkeletalMeshAsset()->Skeleton : nullptr;
    if (!Skeleton || Skeleton->Bones.empty())
    {
        SelectedBoneIndex = -1;
        bRequiredBonesUpdated = true;
        bPoseDirty = bSkinningDirty = false;
        return;
    }

    BoneSpaceTransforms.resize(Skeleton->Bones.size());
    for (int32 i = 0; i < Skeleton->Bones.size(); ++i)
    {
	    BoneSpaceTransforms[i] = TransformFromMatrix(Skeleton->Bones[i].LocalBindPose);
    }

    RefreshBoneTransforms();
}

// 본 인덱스가 할당된 뼈대 배열의 유효 범위 내에 있는지 검사한다.
bool USkeletalMeshComponent::IsValidBoneIndex(int32 BoneIndex) const
{
    if (!SkeletalMeshAsset || !SkeletalMeshAsset->GetSkeletalMeshAsset()) return false;
    return BoneIndex >= 0 && BoneIndex < SkeletalMeshAsset->GetSkeletalMeshAsset()->Skeleton.Bones.size();
}
