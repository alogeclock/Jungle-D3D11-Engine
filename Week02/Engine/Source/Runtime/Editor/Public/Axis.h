#pragma once

#include "Object/Object.h"
#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"

class UAxis : public UObject
{
  public:
    UAxis();
    ~UAxis() override;

    void Render(URenderer &renderer);

  private:
    UAxisComponent *AxisComponent = nullptr;
};