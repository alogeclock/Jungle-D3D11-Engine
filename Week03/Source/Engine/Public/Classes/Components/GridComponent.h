#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UGridComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(UGridComponent, UPrimitiveComponent)

    UGridComponent(const FString &InString);
	virtual ~UGridComponent() override;

protected:
};
