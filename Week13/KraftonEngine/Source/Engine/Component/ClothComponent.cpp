#include "Component/ClothComponent.h"

#include "Engine/Platform/Paths.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/StaticMeshAsset.h"
#include "Physics/Backends/NvClothScene.h"
#include "Physics/Runtime/PhysicsSceneInterface.h"
#include "Profiling/Stats.h"
#include "Render/Proxy/ClothSceneProxy.h"

#include <NvCloth/Cloth.h>

#include <algorithm>
#include <cmath>
#include <cstring>

UClothComponent::UClothComponent()
{
    PrimaryComponentTick.SetTickGroup(TG_PostPhysics);
    PrimaryComponentTick.SetEndTickGroup(TG_PostPhysics);

    BuildDesc.SourceType = EClothSourceType::Grid;
    BuildDesc.GridDesc.PinMode = EClothPinMode::TopRow;

    EnsureMaterialSlotCount(1);
}

UClothComponent::~UClothComponent()
{
    UnregisterClothFromScene();
    ClothInstance.Release();
}

void UClothComponent::BeginPlay()
{
    UMeshComponent::BeginPlay();
    RebuildCloth();
}

void UClothComponent::EndPlay()
{
    UnregisterClothFromScene();
    ClothInstance.Release();
    UMeshComponent::EndPlay();
}

void UClothComponent::CreateRenderState()
{
    if (!ClothInstance.GetCloth())
    {
        RebuildClothInternal(false);
    }

    UPrimitiveComponent::CreateRenderState();
}

FPrimitiveSceneProxy* UClothComponent::CreateSceneProxy()
{
    if (!ClothInstance.GetCloth())
    {
        RebuildClothInternal(false);
    }

    return new FClothSceneProxy(this);
}

void UClothComponent::EnsureMaterialSlotCount(int32 Count)
{
    Count = (std::max)(1, Count);

    if ((int32)MaterialSlots.size() != Count)
    {
        MaterialSlots.resize(Count);
    }

    if ((int32)OverrideMaterials.size() != Count)
    {
        OverrideMaterials.resize(Count, nullptr);
    }

    for (int32 i = 0; i < Count; ++i)
    {
        if (MaterialSlots[i].Path.empty())
        {
            MaterialSlots[i].Path = "None";
        }
    }
}

void UClothComponent::SetStaticMesh(UStaticMesh* InMesh)
{
    StaticMesh = InMesh;
    if (InMesh)
    {
        StaticMesh.SetPath(FPaths::MakeProjectRelative(InMesh->GetAssetPathFileName()));
    }
    else
    {
        StaticMesh.Reset();
    }

    RebuildCloth();
}

UStaticMesh* UClothComponent::GetStaticMesh() const
{
    return StaticMesh;
}

void UClothComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
    EnsureMaterialSlotCount(1);

    if (ElementIndex < 0 || ElementIndex >= (int32)OverrideMaterials.size())
    {
        return;
    }

    OverrideMaterials[ElementIndex] = InMaterial;
    MaterialSlots[ElementIndex].Path = InMaterial ? InMaterial->GetAssetPathFileName() : "None";
    MarkProxyDirty(EDirtyFlag::Material);
}

UMaterial* UClothComponent::GetMaterial(int32 ElementIndex) const
{
    if (ElementIndex >= 0 && ElementIndex < (int32)OverrideMaterials.size())
    {
        return OverrideMaterials[ElementIndex];
    }
    return nullptr;
}

FMeshBuffer* UClothComponent::GetMeshBuffer() const
{
    return nullptr;
}

FMeshDataView UClothComponent::GetMeshDataView() const
{
    const TArray<FVertexPNCTT>& Vertices = ClothInstance.GetRenderVertices();
    const TArray<uint32>& Indices = ClothInstance.GetRenderIndices();
    if (Vertices.empty() || Indices.empty())
    {
        return {};
    }

    FMeshDataView View;
    View.VertexData = Vertices.data();
    View.VertexCount = static_cast<uint32>(Vertices.size());
    View.Stride = sizeof(FVertexPNCTT);
    View.IndexData = Indices.data();
    View.IndexCount = static_cast<uint32>(Indices.size());
    return View;
}

bool UClothComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
    return UPrimitiveComponent::LineTraceComponent(Ray, OutHitResult);
}

bool UClothComponent::GetLocalBounds(FVector& OutCenter, FVector& OutExtent) const
{
    if (!bHasValidBounds)
    {
        return false;
    }

    OutCenter = CachedLocalCenter;
    OutExtent = CachedLocalExtent;
    return true;
}

void UClothComponent::CacheLocalBounds()
{
    bHasValidBounds = ClothInstance.GetLocalBounds(CachedLocalCenter, CachedLocalExtent);
    if (!bHasValidBounds)
    {
        CachedLocalCenter = FVector::ZeroVector;
        CachedLocalExtent = FVector(50.0f, 1.0f, 50.0f);
    }
}

void UClothComponent::UpdateWorldAABB() const
{
    if (!bHasValidBounds)
    {
        UPrimitiveComponent::UpdateWorldAABB();
        return;
    }

    const FMatrix& WorldMatrix = GetWorldMatrix();
    const FVector WorldCenter = WorldMatrix.TransformPositionWithW(CachedLocalCenter);

    const float Ex = std::abs(WorldMatrix.M[0][0]) * CachedLocalExtent.X
        + std::abs(WorldMatrix.M[1][0]) * CachedLocalExtent.Y
        + std::abs(WorldMatrix.M[2][0]) * CachedLocalExtent.Z;
    const float Ey = std::abs(WorldMatrix.M[0][1]) * CachedLocalExtent.X
        + std::abs(WorldMatrix.M[1][1]) * CachedLocalExtent.Y
        + std::abs(WorldMatrix.M[2][1]) * CachedLocalExtent.Z;
    const float Ez = std::abs(WorldMatrix.M[0][2]) * CachedLocalExtent.X
        + std::abs(WorldMatrix.M[1][2]) * CachedLocalExtent.Y
        + std::abs(WorldMatrix.M[2][2]) * CachedLocalExtent.Z;

    WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
    WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}

void UClothComponent::RegisterClothToScene()
{
    if (bRegisteredToClothScene || !ClothInstance.GetCloth())
    {
        return;
    }

    UWorld* World = GetWorld();
    IPhysicsSceneInterface* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    FNvClothScene* ClothScene = PhysicsScene ? PhysicsScene->GetClothScene() : nullptr;
    if (ClothScene && ClothScene->AddInstance(&ClothInstance))
    {
        bRegisteredToClothScene = true;
        World->RegisterClothComponent(this);
    }
}

void UClothComponent::UnregisterClothFromScene()
{
    if (!bRegisteredToClothScene)
    {
        return;
    }

    UWorld* World = GetWorld();
    IPhysicsSceneInterface* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    FNvClothScene* ClothScene = PhysicsScene ? PhysicsScene->GetClothScene() : nullptr;
    if (World)
    {
        World->UnregisterClothComponent(this);
    }
    if (ClothScene)
    {
        ClothScene->RemoveInstance(&ClothInstance);
    }
    bRegisteredToClothScene = false;
}

void UClothComponent::UpdateSimulationSpaceTransform(bool bTeleport)
{
    SCOPE_STAT_CAT("SkinningSync", "Cloth");

    const FMatrix& WorldMatrix = GetWorldMatrix();
    const FVector WorldLocation = WorldMatrix.GetLocation();
    const FQuat WorldRotation = WorldMatrix.ToQuat().GetNormalized();

    if (!bTeleport && bHasPrevSimulationTransform)
    {
        const float DistSq = FVector::DistSquared(WorldLocation, PrevSimulationLocation);
        const float Threshold = BuildDesc.SimulationSettings.TeleportDistanceThreshold;
        if (Threshold > 0.0f && DistSq > Threshold * Threshold)
        {
            bTeleport = true;
        }
    }

    ClothInstance.SetSimulationSpaceTransform(WorldLocation, WorldRotation, bTeleport || !bHasPrevSimulationTransform);
    PrevSimulationLocation = WorldLocation;
    PrevSimulationRotation = WorldRotation;
    bHasPrevSimulationTransform = true;
}

void UClothComponent::ApplyRuntimeClothSettings()
{
    if (!ClothInstance.GetCloth()) return;
    ClothInstance.ApplySimulationSettings(BuildDesc.SimulationSettings, BuildDesc.ConstraintSettings);
    ApplyWindToCloth();
}

void UClothComponent::ApplyWindToCloth()
{
    nv::cloth::Cloth* Cloth = ClothInstance.GetCloth();
    if (!Cloth)
    {
        return;
    }

    FVector SimulationWind = FVector::ZeroVector;
    const bool bHasWind = bEnableWind && WindScale > 0.0f && !WindDirection.IsNearlyZero();
    if (bHasWind)
    {
        const FVector WorldWind = WindDirection.Normalized() * WindScale;
        SimulationWind = GetWorldMatrix().GetInverse().TransformVector(WorldWind);
    }

    // Keep every wind update on one path. NvCloth receives the wind velocity in the
    // same simulation space used by particles and collision primitives.
    // TODO: Non-uniform component scale can skew wind speed; consider a conservative
    // scale policy if scaled cloth components become a supported workflow.
    Cloth->setWindVelocity(physx::PxVec3(SimulationWind.X, SimulationWind.Y, SimulationWind.Z));

    // setWindVelocity only provides air velocity. Drag/lift coefficients decide
    // whether that velocity produces aerodynamic force on triangles.
    Cloth->setDragCoefficient(bHasWind ? 1.0f : 0.0f);
    Cloth->setLiftCoefficient(0.0f);
    Cloth->setFluidDensity(1.0f);
}

void UClothComponent::UpdateClothCollision()
{
    FClothCollisionData CollisionData;

    UWorld* World = GetWorld();
    IPhysicsSceneInterface* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    if (PhysicsScene)
    {
        FClothCollisionGatherParams Params;
        Params.WorldToCloth = GetWorldMatrix().GetInverse();
        Params.IgnoreComponent = this;
        Params.IgnoreActor = GetOwner();
        Params.bIncludeStatic = true;
        Params.bIncludeDynamic = true;
        Params.Thickness = BuildDesc.SimulationSettings.Thickness;
        PhysicsScene->GatherClothCollision(Params, CollisionData);
    }

    ClothInstance.UpdateCollision(CollisionData);
}

void UClothComponent::PrepareClothSimulation(float DeltaTime)
{
    (void)DeltaTime;
    if (!ClothInstance.GetCloth())
    {
        return;
    }

    UpdateSimulationSpaceTransform(false);
    ApplyRuntimeClothSettings();
    UpdateClothCollision();
}

void UClothComponent::FinalizeClothSimulation()
{
    if (!ClothInstance.GetCloth())
    {
        return;
    }

    CacheLocalBounds();
    MarkWorldBoundsDirty();
    MarkProxyDirty(EDirtyFlag::Mesh);
}

void UClothComponent::ApplyMaterialSlots()
{
    EnsureMaterialSlotCount(1);

    for (int32 Index = 0; Index < (int32)MaterialSlots.size(); ++Index)
    {
        const FString& NewMatPath = MaterialSlots[Index].Path;
        if (NewMatPath == "None" || NewMatPath.empty())
        {
            OverrideMaterials[Index] = nullptr;
        }
        else
        {
            OverrideMaterials[Index] = FMaterialManager::Get().GetOrCreateMaterial(NewMatPath);
        }
    }

    MarkProxyDirty(EDirtyFlag::Material);
}

void UClothComponent::RebuildCloth()
{
    RebuildClothInternal(true);
}

bool UClothComponent::RebuildClothInternal(bool bRecreateRenderState)
{
    UnregisterClothFromScene();
    ClothInstance.Release();
    bHasPrevSimulationTransform = false;

    bool bCreated = false;
    if (BuildDesc.SourceType == EClothSourceType::StaticMesh && StaticMesh)
    {
        UStaticMesh* Mesh = GetStaticMesh();
        FStaticMesh* Asset = Mesh ? Mesh->GetStaticMeshAsset() : nullptr;
        if (Asset && !Asset->Vertices.empty() && !Asset->Indices.empty())
        {
            FMeshDataView View;
            View.VertexData = Asset->Vertices.data();
            View.VertexCount = static_cast<uint32>(Asset->Vertices.size());
            View.Stride = sizeof(FNormalVertex);
            View.IndexData = Asset->Indices.data();
            View.IndexCount = static_cast<uint32>(Asset->Indices.size());
            bCreated = ClothInstance.InitializeMesh(BuildDesc, View);
        }
    }

    if (!bCreated)
    {
        FClothBuildDesc GridDesc = BuildDesc;
        GridDesc.SourceType = EClothSourceType::Grid;
        bCreated = ClothInstance.InitializeGrid(GridDesc);
    }

    if (bCreated)
    {
        UpdateSimulationSpaceTransform(true);
        ApplyRuntimeClothSettings();
        RegisterClothToScene();
    }

    CacheLocalBounds();
    MarkWorldBoundsDirty();

    if (bRecreateRenderState)
    {
        MarkRenderStateDirty();
    }
    else
    {
        MarkProxyDirty(EDirtyFlag::Mesh);
    }

    return bCreated;
}

void UClothComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    SCOPE_STAT_CAT("Tick", "Cloth");

    UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!ClothInstance.GetCloth())
    {
        return;
    }

    // Game-world cloth is stepped from UWorld's fixed physics loop:
    // Rigid Simulate -> Fetch -> PrepareClothSimulation -> NvCloth -> Finalize.
    // Tick remains only for editor/debug preview refresh where BeginPlay has not started.
    UWorld* World = GetWorld();
    if (World && World->HasBegunPlay())
    {
        return;
    }

    UpdateSimulationSpaceTransform(false);
    ApplyRuntimeClothSettings();
    CacheLocalBounds();
    MarkWorldBoundsDirty();
    MarkProxyDirty(EDirtyFlag::Mesh);
}

void UClothComponent::PostDuplicate()
{
    UMeshComponent::PostDuplicate();

    if (!StaticMesh.IsNull())
    {
        ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
        UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(StaticMesh.GetPath().ToString(), Device);
        if (Loaded)
        {
            StaticMesh = Loaded;
            StaticMesh.SetPath(FPaths::MakeProjectRelative(Loaded->GetAssetPathFileName()));
        }
    }

    ApplyMaterialSlots();
    RebuildCloth();
}

void UClothComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    const bool bStaticMeshChanged = strcmp(PropertyName, "Static Mesh") == 0;
    const bool bBuildDescChanged = strcmp(PropertyName, "Build Desc") == 0 || strcmp(PropertyName, "Cloth") == 0;
    const bool bRuntimeSettingsChanged = strcmp(PropertyName, "Simulation Settings") == 0
        || strcmp(PropertyName, "Enable CCD") == 0
        || strcmp(PropertyName, "Thickness") == 0
        || strcmp(PropertyName, "Collision Mass Scale") == 0
        || strcmp(PropertyName, "Friction") == 0;
    const bool bWindChanged = strcmp(PropertyName, "Enable Wind") == 0
        || strcmp(PropertyName, "Wind Direction") == 0
        || strcmp(PropertyName, "Wind Scale") == 0;
    const bool bMaterialsChanged = strcmp(PropertyName, "Materials") == 0;
    const bool bMaterialElementChanged = strncmp(PropertyName, "Element ", 8) == 0;

    if (bStaticMeshChanged)
    {
        if (StaticMesh.IsNull())
        {
            StaticMesh.Reset();
        }
        else
        {
            ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
            UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(StaticMesh.GetPath().ToString(), Device);
            if (Loaded)
            {
                StaticMesh = Loaded;
                StaticMesh.SetPath(FPaths::MakeProjectRelative(Loaded->GetAssetPathFileName()));
            }
        }

        RebuildCloth();
        return;
    }

    if (bBuildDescChanged)
    {
        RebuildCloth();
        return;
    }

    if (bRuntimeSettingsChanged || bWindChanged)
    {
        ApplyRuntimeClothSettings();
        UpdateClothCollision();
        return;
    }

    if (bMaterialsChanged || bMaterialElementChanged)
    {
        ApplyMaterialSlots();
    }
}
