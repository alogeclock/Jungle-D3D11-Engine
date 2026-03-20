#pragma once

#include "Source/Runtime/Engine/Object/Public/Actor.h"
#include "Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Source/Runtime/Engine/Public/Classes/Components/GridComponent.h"
#include "Source/Runtime/Core/Public/Math/Matrix.h"

// 일반적인 씬 저장에 포함되지 않도록 에디터 전용 액터로 선언합니다.
class AGrid : public AActor
{
public:
    AGrid(const FString &InString);
    virtual ~AGrid();

    void Render(URenderer& renderer);

private:
    UGridComponent *GridComponent = nullptr;
};