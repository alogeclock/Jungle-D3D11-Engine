#pragma once

#include "Component/UIImageComponent.h"

class UWindowPanelComponent : public UUIImageComponent
{
public:
	DECLARE_CLASS(UWindowPanelComponent, UUIImageComponent)

	UWindowPanelComponent();

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeVisuals(FScene& Scene) const override;

private:
	void ClampSlice();
	void AddPanelQuad(FScene& Scene, ID3D11ShaderResourceView* SRV, float X, float Y, float Width, float Height,
		float U0, float V0, float U1, float V1) const;

private:
	FVector4 Slice = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
	bool bDrawBorder = true;
	bool bDrawCenter = true;
};
