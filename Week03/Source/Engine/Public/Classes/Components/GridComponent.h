#pragma once

#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UGridComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(UGridComponent, UPrimitiveComponent)

    UGridComponent(const FString& InString);
    virtual ~UGridComponent() override;

    float GetGridStep() const;
    void SetGridStep(float InGridStep);
    void Render(URenderer& renderer) override;

private:
    TArray<FBatchedLine> GridLines;
    float GridStep = 1.0f;
    int GridSize = 1000;
    bool bInitialized = false;

    void RebuildGridLines();
};
