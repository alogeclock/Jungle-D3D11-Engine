#include "PhysicalMaterialEditorWidget.h"

#include "Physics/Common/PhysicalMaterialManager.h"
#include "Physics/Common/PhysicsMaterialTypes.h"

#include "ImGui/imgui.h"

namespace
{
	bool RenderCombineModeCombo(const char* Label, EPhysicsMaterialCombineMode& Mode)
	{
		const char* Labels[] = { "Average", "Min", "Multiply", "Max" };
		int CurrentIndex = static_cast<int>(Mode);
		if (ImGui::Combo(Label, &CurrentIndex, Labels, IM_ARRAYSIZE(Labels)))
		{
			Mode = static_cast<EPhysicsMaterialCombineMode>(CurrentIndex);
			return true;
		}
		return false;
	}
}

bool FPhysicalMaterialEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UPhysicalMaterial>();
}

bool FPhysicalMaterialEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UPhysicalMaterial* CurrentAsset = Cast<UPhysicalMaterial>(EditedObject);
	const UPhysicalMaterial* RequestedAsset = Cast<UPhysicalMaterial>(Object);
	return CurrentAsset
		&& RequestedAsset
		&& CurrentAsset->GetAssetPathFileName() == RequestedAsset->GetAssetPathFileName();
}

void FPhysicalMaterialEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	EditedObject = Object;
	bOpen = true;
	ClearDirty();
}

void FPhysicalMaterialEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UPhysicalMaterial* PhysicalMaterial = static_cast<UPhysicalMaterial*>(EditedObject);
	bool bWindowOpen = true;

	FString Title = "Physical Material Editor";
	if (!PhysicalMaterial->GetAssetPathFileName().empty())
	{
		Title += " - ";
		Title += PhysicalMaterial->GetAssetPathFileName();
	}
	if (IsDirty())
	{
		Title += " *";
	}

	ImGui::SetNextWindowSize(ImVec2(760.0f, 820.0f), ImGuiCond_Once);
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin((Title + "###PhysicalMaterialEditor").c_str(), &bWindowOpen, ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::Button("Save"))
	{
		if (FPhysicalMaterialManager::Get().Save(PhysicalMaterial))
		{
			ClearDirty();
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", PhysicalMaterial->GetAssetPathFileName().empty() ? "Unsaved asset" : PhysicalMaterial->GetAssetPathFileName().c_str());
	ImGui::Separator();

	if (RenderDetailsPanel(PhysicalMaterial))
	{
		MarkDirty();
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

bool FPhysicalMaterialEditorWidget::RenderDetailsPanel(UPhysicalMaterial* PhysicalMaterial)
{
	if (!PhysicalMaterial)
	{
		return false;
	}

	bool bChanged = false;

	if (ImGui::BeginTable("PhysicalMaterialDetails", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 280.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		auto BeginRow = [](const char* Label)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(Label);
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
		};

		auto SectionRow = [](const char* Label)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", Label);
			ImGui::TableSetColumnIndex(1);
			ImGui::TextDisabled("");
		};

		SectionRow("Physical Material");

		BeginRow("Friction");
		bChanged |= ImGui::DragFloat("##Friction", &PhysicalMaterial->Friction, 0.01f, 0.0f, 10.0f);

		BeginRow("Static Friction");
		bChanged |= ImGui::DragFloat("##StaticFriction", &PhysicalMaterial->StaticFriction, 0.01f, 0.0f, 10.0f);

		BeginRow("Friction Combine Mode");
		bChanged |= RenderCombineModeCombo("##FrictionCombineMode", PhysicalMaterial->FrictionCombineMode);

		BeginRow("Override Friction Combine Mode");
		bChanged |= ImGui::Checkbox("##OverrideFrictionCombineMode", &PhysicalMaterial->bOverrideFrictionCombineMode);

		BeginRow("Restitution");
		bChanged |= ImGui::DragFloat("##Restitution", &PhysicalMaterial->Restitution, 0.01f, 0.0f, 1.0f);

		BeginRow("Restitution Combine Mode");
		bChanged |= RenderCombineModeCombo("##RestitutionCombineMode", PhysicalMaterial->RestitutionCombineMode);

		BeginRow("Override Restitution Combine Mode");
		bChanged |= ImGui::Checkbox("##OverrideRestitutionCombineMode", &PhysicalMaterial->bOverrideRestitutionCombineMode);

		BeginRow("Density");
		bChanged |= ImGui::DragFloat("##Density", &PhysicalMaterial->Density, 0.01f, 0.0f, 100.0f, "%.2f g/cm3");

		BeginRow("Sleep Linear Velocity Threshold");
		bChanged |= ImGui::DragFloat("##SleepLinearVelocityThreshold", &PhysicalMaterial->SleepLinearVelocityThreshold, 0.01f, 0.0f, 100.0f);

		BeginRow("Sleep Angular Velocity Threshold");
		bChanged |= ImGui::DragFloat("##SleepAngularVelocityThreshold", &PhysicalMaterial->SleepAngularVelocityThreshold, 0.001f, 0.0f, 100.0f);

		BeginRow("Sleep Counter Threshold");
		bChanged |= ImGui::InputInt("##SleepCounterThreshold", &PhysicalMaterial->SleepCounterThreshold);

		SectionRow("Advanced");

		BeginRow("Raise Mass to Power");
		bChanged |= ImGui::SliderFloat("##RaiseMassToPower", &PhysicalMaterial->RaiseMassToPower, 0.0f, 1.0f);

		SectionRow("Physical Properties");

		char SurfaceTypeBuffer[128];
		std::snprintf(SurfaceTypeBuffer, sizeof(SurfaceTypeBuffer), "%s", PhysicalMaterial->SurfaceType.c_str());
		BeginRow("Surface Type");
		if (ImGui::InputText("##SurfaceType", SurfaceTypeBuffer, sizeof(SurfaceTypeBuffer)))
		{
			PhysicalMaterial->SurfaceType = SurfaceTypeBuffer;
			bChanged = true;
		}

		BeginRow("Strength");
		bChanged |= ImGui::DragFloat("##Strength", &PhysicalMaterial->Strength, 0.01f, 0.0f, 1000.0f);

		BeginRow("Damage Modifier");
		bChanged |= ImGui::DragFloat("##DamageModifier", &PhysicalMaterial->DamageModifier, 0.01f, 0.0f, 1000.0f);

		SectionRow("Experimental");

		BeginRow("Show Experimental Properties");
		bChanged |= ImGui::Checkbox("##ShowExperimentalProperties", &PhysicalMaterial->bShowExperimentalProperties);

		int SoftCollisionModeIndex = static_cast<int>(PhysicalMaterial->SoftCollisionMode);
		BeginRow("Soft Collision Mode");
		const char* SoftCollisionLabels[] = { "None", "Simple", "Detailed" };
		if (ImGui::Combo("##SoftCollisionMode", &SoftCollisionModeIndex, SoftCollisionLabels, IM_ARRAYSIZE(SoftCollisionLabels)))
		{
			PhysicalMaterial->SoftCollisionMode = static_cast<EPhysicsMaterialSoftCollisionMode>(SoftCollisionModeIndex);
			bChanged = true;
		}

		BeginRow("Soft Collision Thickness");
		bChanged |= ImGui::DragFloat("##SoftCollisionThickness", &PhysicalMaterial->SoftCollisionThickness, 0.01f, 0.0f, 100.0f);

		BeginRow("Base Friction Impulse");
		bChanged |= ImGui::DragFloat("##BaseFrictionImpulse", &PhysicalMaterial->BaseFrictionImpulse, 0.01f, 0.0f, 10000.0f, "%.2f kgcm");

		ImGui::EndTable();
	}

	return bChanged;
}
