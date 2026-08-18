#pragma once
#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UPlaneComponent : public UPrimitiveComponent
{
public:
	DECLARE_OBJECT(UPlaneComponent, UPrimitiveComponent)

	UPlaneComponent(const FString& InString);
	virtual ~UPlaneComponent() override;

    virtual void Submit(const FSceneViewOptions& ViewOptions) override;

protected:
};