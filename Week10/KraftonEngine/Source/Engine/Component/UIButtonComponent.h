#pragma once

#include "Component/UIImageComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class UIButtonComponent : public UUIImageComponent
{
public:
	DECLARE_CLASS(UIButtonComponent, UUIImageComponent)

	UIButtonComponent();

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void ContributeVisuals(FScene& Scene) const override;

	const FString& GetLabel() const { return Label; }
	void SetLabel(const FString& InLabel) { Label = InLabel; }
	void SetFont(const FName& InFontName);
	const FName& GetFontName() const { return FontName; }
	const FFontResource* GetFont() const { return CachedFont; }

	bool IsHovered() const { return bHovered; }
	bool IsPressed() const { return bPressed; }
	bool WasClicked() const { return bClickedThisFrame; }

private:
	FVector4 GetCurrentTint() const;
	bool IsPointInsideButton(float X, float Y) const;

private:
	FString Label = "Button";
	FName FontName = FName("Default");
	FFontResource* CachedFont = nullptr;
	FVector4 LabelColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector LabelOffset = FVector(24.0f, 18.0f, 0.0f);
	float LabelScale = 1.0f;
	FVector4 NormalTint = FVector4(1.0f, 1.0f, 1.0f, 0.95f);
	FVector4 HoverTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 PressedTint = FVector4(0.85f, 0.85f, 0.85f, 1.0f);
	bool bHovered = false;
	bool bPressed = false;
	bool bClickedThisFrame = false;
};
