#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UCubeComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(UCubeComponent, UPrimitiveComponent)

    UCubeComponent(const FString &InString);
	virtual ~UCubeComponent() override;

protected:
};
