
#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SceneComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/MeshManager.h"

class URenderer;

class UPrimitiveComponent : public USceneComponent
{
  public:
    UPrimitiveComponent() {}
    virtual ~UPrimitiveComponent();

    virtual void Render(URenderer &renderer);

    void           SetPrimitiveType(EPrimitiveType InType) { PrimitiveType = InType; }
    EPrimitiveType GetPrimitiveType() const { return PrimitiveType; }

    void                     SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology) { Topology = InTopology; }
    D3D11_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }

  protected:
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    EPrimitiveType           PrimitiveType = EPrimitiveType::None;
    bool                     bEnableDepthTest = false;
};