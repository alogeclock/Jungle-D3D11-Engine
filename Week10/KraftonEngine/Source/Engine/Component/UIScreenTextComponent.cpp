#include "Component/UIScreenTextComponent.h"

#include "Render/Scene/FScene.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UUIScreenTextComponent, UBillboardComponent)

UUIScreenTextComponent::UUIScreenTextComponent()
{
	SetFont(FontName);
	SetCanDeleteFromDetails(true);
}

void UUIScreenTextComponent::Serialize(FArchive& Ar)
{
	UBillboardComponent::Serialize(Ar);
	Ar << Text;
	Ar << FontName;
	Ar << ScreenPosition;
	Ar << Color;
	Ar << FontSize;
}

void UUIScreenTextComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Text", EPropertyType::String, &Text });
	OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
	OutProps.push_back({ "Screen Position", EPropertyType::Vec3, &ScreenPosition, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Color", EPropertyType::Color4, &Color });
	OutProps.push_back({ "Font Size", EPropertyType::Float, &FontSize, 0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UUIScreenTextComponent::PostEditProperty(const char* PropertyName)
{
	UBillboardComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Font") == 0)
	{
		SetFont(FontName);
	}
}

void UUIScreenTextComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	Scene.AddScreenText(Text, FVector2(ScreenPosition.X, ScreenPosition.Y), FontSize, Color);
}

void UUIScreenTextComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}
