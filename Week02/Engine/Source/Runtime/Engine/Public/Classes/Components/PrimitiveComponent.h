
#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SceneComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/MeshManager.h"
#include "Engine/Source/Runtime/Core/Public/Math/Intersections.h"

class URenderer;

struct FHitResult
{
    bool                 bHit = false;
    float                Distance = FLT_MAX;
    FVector<float>       HitPoint = {};
    UPrimitiveComponent *HitComponent = nullptr;
};

class UPrimitiveComponent : public USceneComponent
{
public:
    UPrimitiveComponent();
    virtual ~UPrimitiveComponent();

    virtual void Render(URenderer &renderer);
    void         Selected();
    void         NotSelected();

    void           SetPrimitiveType(EPrimitiveType InType) { PrimitiveType = InType; }
    EPrimitiveType GetPrimitiveType() const { return PrimitiveType; }

    void                     SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology) { Topology = InTopology; }
    D3D11_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }

    void      SetCullMode(ECullMode InCullMode) { CullMode = InCullMode; }
    ECullMode GetCullMode() const { return CullMode; }

    bool isAlwaysVisible() const { return !bEnableDepthTest; }
    void SetAlwaysVisible(const bool bInEnableDepthTest) { bEnableDepthTest = !bInEnableDepthTest; }

    virtual FHitResult IntersectRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);


    static UObject *ConstructPrimitiveComponent() { return new UPrimitiveComponent(); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("UPrimitiveComponent", USceneComponent::StaticClass(), &UPrimitiveComponent::ConstructPrimitiveComponent);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

protected:
    FVector<float> GetLocalAABBMin() const;
    FVector<float> GetLocalAABBMax() const;

private:
    FTransform GetTransformFromOwner() const;
    bool IntersectRayBoundingSphere(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
    bool IntersectRayAABB(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
    FHitResult IntersectRayMeshTriangle(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

protected:
    EPrimitiveType           PrimitiveType = EPrimitiveType::None;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ECullMode                CullMode = ECullMode::Back;
    bool                     bEnableDepthTest = true;
};