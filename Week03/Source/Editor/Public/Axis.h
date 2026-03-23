#pragma once

#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Object/Public/Actor.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/Classes/Components/AxisComponent.h"

class AAxis : public AActor
{
  public:
    AAxis(const FString &InString);
    ~AAxis() override;

  private:
      UAxisComponent *AxisComponent = nullptr;
};