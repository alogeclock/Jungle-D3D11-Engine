// Copyright Epic Games, Inc. All Rights Reserved.
#include "ShapeComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Proxy/ShapeSceneProxy.h"

#include <cstring>

IMPLEMENT_CLASS(UShapeComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UShapeComponent)

BEGIN_CLASS_PROPERTIES(UShapeComponent)
	REGISTER_PROPERTY(ShapeColor, "Shape Color", EPropertyType::Color4, "Shape", CPF_Edit)
	PROPERTY_BOOL(bDrawOnlyIfSelected, "Draw Only If Selected", "Shape", CPF_Edit)
END_CLASS_PROPERTIES(UShapeComponent)

UShapeComponent::UShapeComponent()
{
	bCastShadow = false;
}

FPrimitiveSceneProxy* UShapeComponent::CreateSceneProxy()
{
	return new FShapeSceneProxy(this);
}

void UShapeComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Shape Color") == 0 || strcmp(PropertyName, "Draw Only If Selected") == 0)
	{
		MarkRenderStateDirty();
	}
}

void UShapeComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << ShapeColor;
	Ar << bDrawOnlyIfSelected;
}
