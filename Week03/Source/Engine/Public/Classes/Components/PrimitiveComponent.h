
#pragma once

#include "Source/Engine/Public/Classes/Components/SceneComponent.h"
#include "Source/Engine/Public/Classes/MeshManager.h"
#include "Source/Engine/Object/Public/Actor.h"
#include "Source/Core/Public/Math/Intersections.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Box.h"
#include "Source/Core/Public/Memory.h"

#include "Source/Engine/Public/Rendering/RenderProxy.h"

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
    
    // 팩토리 메서드: 자식 클래스(Cube, LineBatcher 등)에서 자신에게 맞는 Proxy를 생성하도록 강제
    virtual void Submit(const FSceneViewOptions& ViewOptions);
    virtual FRenderProxy* CreateRenderProxy();

    virtual void Render(URenderer &renderer);
    virtual void SetSelectEffect(bool Selected);

    void           SetPrimitiveType(EPrimitiveType InType);
    EPrimitiveType GetPrimitiveType() const { return PrimitiveType; }

    void                     SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology) { Topology = InTopology; }
    D3D11_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }

    void      SetCullMode(ECullMode InCullMode) { CullMode = InCullMode; }
    ECullMode GetCullMode() const { return CullMode; }

    void SetEnableDepthTest(const bool bInEnableDepthTest) { bEnableDepthTest = bInEnableDepthTest; }
    bool GetEnableDepthTest() const { return bEnableDepthTest; }

    void SetIsInEditor(bool IsInEditor) { bIsInEditor = IsInEditor; }
    bool IsInEditor() const { return bIsInEditor; }

    bool IsRenderable(URenderer &renderer);

    virtual void UpdateBounds();
    virtual void UpdateBoundsTexture(TArray<FTextureVertex>* vertices);
    const FBox  &GetWorldAABB();

    virtual FHitResult IntersectRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

    void SetVisible(bool bVisible) { bIsVisible = bVisible; }
    bool IsVisible() const { return bIsVisible; }

  protected:
    FTransform GetTransformFromOwner() const;

  private:
    bool       IntersectRayBoundingSphere(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
    bool       IntersectRayAABB(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
    virtual FHitResult IntersectRayMeshTriangle(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

  protected:
    FRenderProxy* RenderProxy = nullptr;

    EPrimitiveType           PrimitiveType = EPrimitiveType::None;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ECullMode                CullMode = ECullMode::Back;
    bool                     bEnableDepthTest = true;
    bool                     bLocalBoundsDirty = true;
    bool                     bIsInEditor = false;
    bool                     bIsTextured = false;
    bool                     bIsVisible = true;
    bool                     bShowAABB = false;
    bool                     bShowUUID = false;

    FBox LocalAABB;
    FBox WorldAABB;
};