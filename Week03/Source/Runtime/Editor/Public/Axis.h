#pragma once

#include "Source/Runtime/Engine/Object/Public/Object.h"
#include "Source/Runtime/Engine/Object/Public/Actor.h"
#include "Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"

class AAxis : public AActor
{
  public:
    AAxis(const FString &InString);
    ~AAxis() override;

    void Render(URenderer &renderer);

  private:
    UAxisComponent *AxisComponent = nullptr;
};