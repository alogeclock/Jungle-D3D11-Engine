#pragma once

#include "Core/CoreTypes.h"
#include "Editor/Settings/EditorSettings.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"

struct FPreviewSettings
{
	float CameraSpeed = 10.0f;
	float CameraRotationSpeed = 60.0f;
	float CameraZoomSpeed = 300.0f;
	FVector InitViewPos = FVector(10.0f, 0.0f, 5.0f);
	FVector InitLookAt = FVector(0.0f, 0.0f, 0.0f);

	EEditorCoordSystem CoordSystem = EEditorCoordSystem::World;
	bool bEnableTranslationSnap = false;
	float TranslationSnapSize = 0.1f;
	bool bEnableRotationSnap = false;
	float RotationSnapSize = 15.0f;
	bool bEnableScaleSnap = false;
	float ScaleSnapSize = 0.1f;

	FViewportRenderOptions RenderOptions;

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath();
};
