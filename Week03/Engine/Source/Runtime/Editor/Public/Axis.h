#pragma once

#include "Object/Object.h"
#include "Object/Actor.h"
#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"

class AAxis : public AActor
{
  public:
    AAxis(const FString &InString);
    ~AAxis() override;

    void Render(URenderer &renderer);

  private:
    UAxisComponent *AxisComponent = nullptr;
};