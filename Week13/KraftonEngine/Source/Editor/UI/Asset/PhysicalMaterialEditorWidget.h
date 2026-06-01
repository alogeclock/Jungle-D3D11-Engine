#pragma once

#include "AssetEditorWidget.h"

class UPhysicalMaterial;

class FPhysicalMaterialEditorWidget : public FAssetEditorWidget
{
public:
	bool CanEdit(UObject* Object) const override;
	bool AllowsMultipleInstances() const override { return true; }
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Render(float DeltaTime) override;

private:
	bool RenderDetailsPanel(UPhysicalMaterial* PhysicalMaterial);
};

