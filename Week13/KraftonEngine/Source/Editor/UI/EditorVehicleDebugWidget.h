#pragma once

#include "Editor/UI/EditorWidget.h"

class UVehicleMovementComponent;

class EditorVehicleDebugWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;

private:
	UVehicleMovementComponent* FindVehicleMovement() const;
	void RenderVehicleStats(UVehicleMovementComponent* VehicleMovement) const;
};
