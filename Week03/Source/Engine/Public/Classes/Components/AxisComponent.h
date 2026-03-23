#pragma once

#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UAxisComponent : public UPrimitiveComponent
{
public:
	DECLARE_OBJECT(UAxisComponent, UPrimitiveComponent)

	UAxisComponent(const FString& InString);
	virtual ~UAxisComponent() override;

	void  SetGridStep(float InGridStep);
	void Render(URenderer& renderer) override;

private:
	TArray<FBatchedLine> AxisLines;
	float                GridStep = 1.0f;
	// Render가 처음 호출될 때 AxisLines를 초기화하기 위한 플래그
	bool bInitialized = false;
	void RebuildAxisLines();
};