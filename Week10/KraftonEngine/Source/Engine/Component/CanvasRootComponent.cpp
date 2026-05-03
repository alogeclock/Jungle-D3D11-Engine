#include "Component/CanvasRootComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UCanvasRootComponent, USceneComponent)

void UCanvasRootComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << CanvasSize;
}

void UCanvasRootComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Canvas Size", EPropertyType::Vec3, &CanvasSize, 0.0f, 4096.0f, 1.0f });
}

void UCanvasRootComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	(void)PropertyName;
}
