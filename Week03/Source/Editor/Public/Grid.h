#pragma once

#include "Source/Engine/Object/Public/Actor.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/Classes/Components/GridComponent.h"
#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"

// 일반적인 씬 저장에 포함되지 않도록 에디터 전용 액터로 선언합니다.
class AGrid : public AActor
{
  public:
    AGrid(const FString &InString);
    virtual ~AGrid();

    void Render(URenderer &renderer);

    void SetGridStep(float inGridStep)
    {
        GridStep = inGridStep;
        GridComponent->SetScale({GridStep, GridStep, GridStep});
    }

    float GetGridStep() const { return GridStep; }

  private:
    UGridComponent *GridComponent = nullptr;
    float           GridStep = 1.0f;
};