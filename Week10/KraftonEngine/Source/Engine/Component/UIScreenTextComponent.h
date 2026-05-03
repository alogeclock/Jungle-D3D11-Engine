#pragma once

#include "Component/BillboardComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class FScene;
struct FPropertyDescriptor;

class UUIScreenTextComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(UUIScreenTextComponent, UBillboardComponent)

	UUIScreenTextComponent();

	FPrimitiveSceneProxy* CreateSceneProxy() override { return nullptr; }
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeVisuals(FScene& Scene) const override;

	void SetText(const FString& InText) { Text = InText; }
	const FString& GetText() const { return Text; }

	void SetFont(const FName& InFontName);
	const FName& GetFontName() const { return FontName; }
	const FFontResource* GetFont() const { return CachedFont; }

	void SetScreenPosition(const FVector& InScreenPosition) { ScreenPosition = InScreenPosition; }
	const FVector& GetScreenPosition() const { return ScreenPosition; }

	void SetColor(const FVector4& InColor) { Color = InColor; }
	const FVector4& GetColor() const { return Color; }

	void SetFontSize(float InSize) { FontSize = InSize; }
	float GetFontSize() const { return FontSize; }

private:
	FString Text = FString("Screen Text");
	FName FontName = FName("Default");
	FFontResource* CachedFont = nullptr;
	FVector ScreenPosition = FVector(32.0f, 32.0f, 0.0f);
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float FontSize = 1.0f;
};
