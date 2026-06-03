#include "Editor/UI/EditorVehicleDebugWidget.h"

#include "Editor/EditorEngine.h"
#include "Component/VehicleMovementComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Physics/Systems/Vehicle/VehicleDebugTypes.h"
#include "ImGui/imgui.h"

#include <cstdarg>
#include <cstdio>

namespace
{
void FormatVehicleGear(char* Buffer, int32 BufferSize, int32 Gear)
{
	if (!Buffer || BufferSize <= 0)
	{
		return;
	}

	if (Gear == 0)
	{
		snprintf(Buffer, BufferSize, "%d (R)", Gear);
	}
	else if (Gear == 1)
	{
		snprintf(Buffer, BufferSize, "%d (N)", Gear);
	}
	else
	{
		snprintf(Buffer, BufferSize, "%d (%d)", Gear, Gear - 1);
	}
}

void DrawVehicleStatRow(const char* Label, const char* ValueFmt, ...)
{
	char Value[128] = {};
	va_list Args;
	va_start(Args, ValueFmt);
	vsnprintf(Value, sizeof(Value), ValueFmt, Args);
	va_end(Args);

	ImGui::TextDisabled("%s", Label);
	ImGui::SameLine(150.0f);
	ImGui::Text("%s", Value);
}
}

void EditorVehicleDebugWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(360.0f, 360.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Vehicle Debug"))
	{
		ImGui::End();
		return;
	}

	UVehicleMovementComponent* VehicleMovement = FindVehicleMovement();
	if (!VehicleMovement)
	{
		ImGui::TextDisabled("Vehicle: not found");
		ImGui::End();
		return;
	}

	AActor* Owner = VehicleMovement->GetOwner();
	ImGui::Text("Target: %s", Owner ? Owner->GetName().c_str() : VehicleMovement->GetName().c_str());

	bool bDrawDebug = VehicleMovement->IsDrawDebugEnabled();
	if (ImGui::Checkbox("Draw Vehicle Debug", &bDrawDebug))
	{
		VehicleMovement->SetDrawDebugEnabled(bDrawDebug);
	}

	ImGui::Separator();
	RenderVehicleStats(VehicleMovement);

	ImGui::End();
}

UVehicleMovementComponent* EditorVehicleDebugWidget::FindVehicleMovement() const
{
	UWorld* World = EditorEngine ? EditorEngine->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	AActor* Selected = EditorEngine ? EditorEngine->GetSelectionManager().GetPrimarySelection() : nullptr;
	if (Selected && IsAliveObject(Selected))
	{
		if (UVehicleMovementComponent* VehicleMovement = Selected->GetComponentByClass<UVehicleMovementComponent>())
		{
			return VehicleMovement;
		}
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		if (UVehicleMovementComponent* VehicleMovement = Actor->GetComponentByClass<UVehicleMovementComponent>())
		{
			return VehicleMovement;
		}
	}

	return nullptr;
}

void EditorVehicleDebugWidget::RenderVehicleStats(UVehicleMovementComponent* VehicleMovement) const
{
	if (!VehicleMovement)
	{
		return;
	}

	const FVehicleRuntimeStats* Stats = VehicleMovement->GetRuntimeStats();
	if (!Stats || !VehicleMovement->IsVehicleValid())
	{
		ImGui::TextDisabled("Runtime stats are unavailable before vehicle simulation starts.");
		return;
	}

	int32 GroundedWheelCount = 0;
	for (const FVehicleWheelDebugState& Wheel : Stats->Wheels)
	{
		if (!Wheel.bInAir)
		{
			++GroundedWheelCount;
		}
	}

	char GearText[32] = {};
	FormatVehicleGear(GearText, sizeof(GearText), VehicleMovement->GetCurrentGear());

	DrawVehicleStatRow("Speed", "%.2f km/h", Stats->CurrentSpeed);
	DrawVehicleStatRow("RPM", "%.0f", Stats->EngineRpm);
	DrawVehicleStatRow("Gear", "%s", GearText);
	DrawVehicleStatRow("Throttle", "%.2f", Stats->InputState.Throttle);
	DrawVehicleStatRow("Brake", "%.2f", Stats->InputState.Brake);
	DrawVehicleStatRow("Steering", "%.2f", Stats->InputState.Steering);
	DrawVehicleStatRow("Handbrake", "%s", Stats->InputState.bHandbrake ? "on" : "off");
	DrawVehicleStatRow("Applied Accel", "%.2f", Stats->EngineTorque);
	DrawVehicleStatRow("Applied Brake", "%.2f", Stats->BrakeTorque);
	DrawVehicleStatRow("Grounded Wheels", "%d / %d", GroundedWheelCount, Stats->WheelCount);
	DrawVehicleStatRow("In Air", "%s", Stats->bInAir ? "true" : "false");
}
