#pragma once

#include "Component/BillboardComponent.h"

class FScene;

class UUIImageComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(UUIImageComponent, UBillboardComponent)

	UUIImageComponent();

	FPrimitiveSceneProxy* CreateSceneProxy() override { return nullptr; }
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeVisuals(FScene& Scene) const override;

	const FVector& GetScreenPosition() const { return ScreenPosition; }
	void SetScreenPosition(const FVector& InScreenPosition) { ScreenPosition = InScreenPosition; }

	const FVector4& GetTint() const { return Tint; }
	void SetTint(const FVector4& InTint) { Tint = InTint; }

	int32 GetZOrder() const { return ZOrder; }
	void SetZOrder(int32 InZOrder) { ZOrder = InZOrder; }

	const FVector& GetScreenSize() const { return ScreenSize; }
	void SetScreenSize(const FVector& InScreenSize);

	const FString& GetTexturePath() const { return TextureSlot.Path; }
	bool SetTexturePath(const FString& InTexturePath);

protected:
	bool EnsureTextureLoaded();
	ID3D11ShaderResourceView* GetResolvedTextureSRV() const;

protected:
	FVector ScreenPosition = FVector(0.0f, 0.0f, 0.0f);
	FVector ScreenSize = FVector(256.0f, 128.0f, 0.0f);
	FVector4 Tint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	int32 ZOrder = 0;
};
