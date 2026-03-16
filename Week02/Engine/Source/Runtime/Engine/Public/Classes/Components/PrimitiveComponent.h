
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
    UPrimitiveComponent() {}
    virtual ~UPrimitiveComponent();

    virtual void Render(URenderer &renderer);
    void         Selected();
    void         NotSelected();

    void           SetPrimitiveType(EPrimitiveType InType) { PrimitiveType = InType; }
    EPrimitiveType GetPrimitiveType() const { return PrimitiveType; }

    void                     SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology) { Topology = InTopology; }
    D3D11_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }

    bool isAlwaysVisible() const { return bEnableDepthTest; }
    void SetAlwaysVisible(const bool bInEnableDepthTest) { bEnableDepthTest = !bInEnableDepthTest; }

    virtual FHitResult IntersectRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

  protected:
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    EPrimitiveType           PrimitiveType = EPrimitiveType::None;
    bool                     bEnableDepthTest = true;
    float                    ColorAlpha = 1.0f;
};