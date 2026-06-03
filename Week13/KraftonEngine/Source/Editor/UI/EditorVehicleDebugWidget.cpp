#include "Editor/UI/EditorVehicleDebugWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
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

	bool& bVehicleDebugOpen = FEditorSettings::Get().UI.bVehicleDebug;
	ImGui::SetNextWindowSize(ImVec2(360.0f, 360.0f), ImGuiCond_FirstUseEver);
	constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	if (!ImGui::Begin("Vehicle Debug", &bVehicleDebugOpen, WindowFlags))
	{
		ImGui::End();
		return;
	}

	AActor* TargetActor = nullptr;
	int32 MatchCount = 0;
	int32 SafeMatchIndex = 0;
	UVehicleMovementComponent* VehicleMovement = FindVehicleMovement(TargetActor, MatchCount, SafeMatchIndex);
	MatchIndex = SafeMatchIndex;

	const bool bHasMatches = MatchCount > 0;
	if (!VehicleMovement)
	{
		ImGui::TextDisabled("Target: not found");
		ImGui::Text("Matches: %d", MatchCount);
		ImGui::BeginDisabled(true);
		ImGui::Button("Prev");
		ImGui::SameLine();
		ImGui::Button("Next");
		ImGui::EndDisabled();
		ImGui::End();
		return;
	}

	ImGui::Text("Target: %s (%d / %d)",
		TargetActor ? TargetActor->GetName().c_str() : VehicleMovement->GetName().c_str(),
		MatchIndex + 1,
		MatchCount);
	ImGui::Text("Matches: %d", MatchCount);

	ImGui::BeginDisabled(!bHasMatches);
	if (ImGui::Button("Prev") && bHasMatches)
	{
		MatchIndex = (MatchIndex - 1 + MatchCount) % MatchCount;
		VehicleMovement = FindVehicleMovement(TargetActor, MatchCount, SafeMatchIndex);
		MatchIndex = SafeMatchIndex;
	}
	ImGui::SameLine();
	if (ImGui::Button("Next") && bHasMatches)
	{
		MatchIndex = (MatchIndex + 1) % MatchCount;
		VehicleMovement = FindVehicleMovement(TargetActor, MatchCount, SafeMatchIndex);
		MatchIndex = SafeMatchIndex;
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	RenderVehicleStats(VehicleMovement);

	ImGui::End();
}

TArray<EditorVehicleDebugWidget::FVehicleMatch> EditorVehicleDebugWidget::CollectVehicleMatches() const
{
	TArray<FVehicleMatch> Matches;
	UWorld* World = EditorEngine ? EditorEngine->GetWorld() : nullptr;
	if (!World)
	{
		return Matches;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		if (UVehicleMovementComponent* VehicleMovement = Actor->GetComponentByClass<UVehicleMovementComponent>())
		{
			Matches.push_back({ Actor, VehicleMovement });
		}
	}

	return Matches;
}

UVehicleMovementComponent* EditorVehicleDebugWidget::FindVehicleMovement(AActor*& OutActor, int32& OutMatchCount, int32& OutMatchIndex)
{
	OutActor = nullptr;
	OutMatchCount = 0;
	OutMatchIndex = 0;

	TArray<FVehicleMatch> Matches = CollectVehicleMatches();
	OutMatchCount = static_cast<int32>(Matches.size());
	if (Matches.empty())
	{
		return nullptr;
	}

	const int32 SafeIndex = ((MatchIndex % OutMatchCount) + OutMatchCount) % OutMatchCount;
	OutMatchIndex = SafeIndex;
	OutActor = Matches[SafeIndex].Actor;
	return Matches[SafeIndex].VehicleMovement;
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
