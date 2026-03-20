#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UArrowComponent : public UPrimitiveComponent
{
public:
	DECLARE_OBJECT(UArrowComponent, UPrimitiveComponent)

	UArrowComponent(const FString& InString);
	virtual ~UArrowComponent() override;

protected:
};