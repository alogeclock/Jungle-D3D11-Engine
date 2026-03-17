#pragma once

#include "Object/Actor.h"
#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/GridComponent.h"
#include "Engine/Source/Runtime/Core/Public/Math/Matrix.h"

// 일반적인 씬 저장에 포함되지 않도록 에디터 전용 액터로 선언합니다.
class AGrid : public AActor
{
public:
    AGrid();
    virtual ~AGrid();

    void Render(URenderer& renderer);

private:
    UGridComponent *GridComponent = nullptr;
};