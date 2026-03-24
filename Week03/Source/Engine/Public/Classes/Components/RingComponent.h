#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class URingComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT(URingComponent, UPrimitiveComponent)

public:
    URingComponent(const FString &InString);
	virtual ~URingComponent() override;

    virtual void Submit() override;

protected:
};