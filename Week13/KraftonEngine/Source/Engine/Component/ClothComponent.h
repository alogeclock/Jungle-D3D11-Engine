#pragma once

#include "Object/ObjectMacros.h"
#include "Component/MeshComponent.h"
#include "Core/Property/PropertyTypes.h"
#include "Core/UObject/TSoftObjectPtr.h"
#include "Physics/Systems/Cloth/ClothInstance.h"
#include "Mesh/MeshManager.h"
#include "Mesh/StaticMesh.h"
#include "ClothComponent.generated.h"

class UMaterial;
class FPrimitiveSceneProxy;

UCLASS()
class UClothComponent : public UMeshComponent
{
public:
    GENERATED_BODY(UClothComponent)

    UClothComponent();
    ~UClothComponent() override = default;

    void BeginPlay() override;
    void EndPlay() override;
    void CreateRenderState() override;

    FMeshBuffer* GetMeshBuffer() const override;
    FMeshDataView GetMeshDataView() const override;
    bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
    void UpdateWorldAABB() const override;
    bool CanCreatePhysicsBody() const override { return false; }

    FPrimitiveSceneProxy* CreateSceneProxy() override;

    void SetStaticMesh(UStaticMesh* InMesh);
    UStaticMesh* GetStaticMesh() const;
    bool GetLocalBounds(FVector& OutCenter, FVector& OutExtent) const;

    void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
    UMaterial* GetMaterial(int32 ElementIndex) const;
    const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

    void PostDuplicate() override;
    void PostEditProperty(const char* PropertyName) override;

    FString GetStaticMeshPath() const { return StaticMesh.GetPath().ToString(); }

    void RebuildCloth();
    const FClothInstance& GetClothInstance() const { return ClothInstance; }
    FClothInstance& GetClothInstance() { return ClothInstance; }

protected:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
    bool RebuildClothInternal(bool bRecreateRenderState);
    void CacheLocalBounds();
    void RegisterClothToScene();
    void UnregisterClothFromScene();
    void UpdateSimulationSpaceTransform(bool bTeleport);
    void EnsureMaterialSlotCount(int32 Count);
    void ApplyMaterialSlots();

private:
    UPROPERTY(Edit, Category = "Mesh", DisplayName = "Static Mesh", Type = SoftObject, Class = UStaticMesh)
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Build Desc", Type = Struct, Struct = FClothBuildDesc)
    FClothBuildDesc BuildDesc;

    UPROPERTY(Edit, FixedSize, Category = "Materials", DisplayName = "Materials")
    TArray<FMaterialSlot> MaterialSlots;

    TArray<UMaterial*> OverrideMaterials;
    FClothInstance ClothInstance;

    FVector CachedLocalCenter = FVector::ZeroVector;
    FVector CachedLocalExtent = FVector(50.0f, 1.0f, 50.0f);
    bool bHasValidBounds = false;
    bool bRegisteredToClothScene = false;
    bool bHasPrevSimulationTransform = false;
    FVector PrevSimulationLocation = FVector::ZeroVector;
    FQuat PrevSimulationRotation = FQuat::Identity;
};
