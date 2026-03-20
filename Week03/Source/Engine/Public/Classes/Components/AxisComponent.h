#pragma once

#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UAxisComponent : public UPrimitiveComponent
{
public:
	DECLARE_OBJECT(UAxisComponent, UPrimitiveComponent)

	UAxisComponent(const FString& InString);
	virtual ~UAxisComponent() override;

protected:
};