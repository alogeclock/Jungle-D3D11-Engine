
#pragma once

#include "Source/Engine/Public/Classes/Components/SceneComponent.h"
#include "Source/Engine/Public/Classes/MeshManager.h"
#include "Source/Engine/Object/Public/Actor.h"
#include "Source/Core/Public/Math/Intersections.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Box.h"
#include "Source/Core/Public/Memory.h"

#include <iostream>

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
    DECLARE_OBJECT_START(UPrimitiveComponent, USceneComponent)
    DECLARE_END

  public:
    UPrimitiveComponent(const FString &InString);
    virtual ~UPrimitiveComponent() override;

    virtual void Render(URenderer &renderer);
    void         Selected();
    void         NotSelected();

    void           SetPrimitiveType(EPrimitiveType InType);
    EPrimitiveType GetPrimitiveType() const { return PrimitiveType; }

    void                     SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology) { Topology = InTopology; }
    D3D11_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }

    void      SetCullMode(ECullMode InCullMode) { CullMode = InCullMode; }
    ECullMode GetCullMode() const { return CullMode; }

    bool isAlwaysVisible() const { return !bEnableDepthTest; }
    void SetAlwaysVisible(const bool bInEnableDepthTest) { bEnableDepthTest = !bInEnableDepthTest; }

    void SetIsInEditor(bool IsInEditor) { bIsInEditor = IsInEditor; }
    bool IsInEditor() const { return bIsInEditor; }

    virtual void UpdateBounds();
    const FBox  &GetWorldAABB();

    virtual FHitResult IntersectRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

  protected:
    FTransform GetTransformFromOwner() const;

  private:
    bool       IntersectRayBoundingSphere(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
    bool       IntersectRayAABB(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
    FHitResult IntersectRayMeshTriangle(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

  protected:
    EPrimitiveType           PrimitiveType = EPrimitiveType::None;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ECullMode                CullMode = ECullMode::Back;

    bool bEnableDepthTest = true;
    bool bLocalBoundsDirty = true;
    bool bIsInEditor = false;

    FBox LocalAABB;
    FBox WorldAABB;

    FVector<float> Corners[8];
};