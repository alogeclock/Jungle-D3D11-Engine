#pragma once

#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UGridComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(UGridComponent, UPrimitiveComponent)

    UGridComponent(const FString& InString);
    virtual ~UGridComponent() override;

    void SetGridStep(float InGridStep);
    void Render(URenderer& renderer) override;

    virtual void Submit(const FSceneViewOptions& ViewOptions) override;

private:
    TArray<FBatchedLine> GridLines;
    float GridStep = 1.0f;
    int GridSize = 1000;
    bool bNeedRebuild = true;

#ifdef _DEBUG
    ID3D11Buffer* DynamicVertexBuffer = nullptr;
    uint32 VertexBufferSize = 0;
#endif

    void RebuildGridLines();
};
