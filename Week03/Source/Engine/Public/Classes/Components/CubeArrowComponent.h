#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UCubeArrowComponent : public UPrimitiveComponent
{
public:
	DECLARE_OBJECT(UCubeArrowComponent, UPrimitiveComponent)

	UCubeArrowComponent(const FString& InString);
	virtual ~UCubeArrowComponent() override;

    virtual void Submit(const FSceneViewOptions& ViewOptions) override;

protected:
};