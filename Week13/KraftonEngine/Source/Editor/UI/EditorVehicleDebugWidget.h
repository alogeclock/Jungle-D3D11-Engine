#pragma once

#include "Editor/UI/EditorWidget.h"

class AActor;
class UVehicleMovementComponent;

class EditorVehicleDebugWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;
	int32 GetMatchIndex() const { return MatchIndex; }

private:
	struct FVehicleMatch
	{
		AActor* Actor = nullptr;
		UVehicleMovementComponent* VehicleMovement = nullptr;
	};

	TArray<FVehicleMatch> CollectVehicleMatches() const;
	UVehicleMovementComponent* FindVehicleMovement(AActor*& OutActor, int32& OutMatchCount, int32& OutMatchIndex);
	void RenderVehicleStats(UVehicleMovementComponent* VehicleMovement) const;

	int32 MatchIndex = 0;
};
