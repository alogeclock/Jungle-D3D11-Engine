#include "PreviewViewportWidget.h"

#include "Component/CameraComponent.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/Viewport/Preview/PreviewViewportClient.h"
#include "Math/MathUtils.h"
#include "Resource/ResourceManager.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <d3d11.h>
#include <algorithm>
#include <cstdio>

namespace
{
	enum class EPreviewToolbarIcon : int32
	{
		Translate,
		Rotate,
		Scale,
		WorldSpace,
		LocalSpace,
		Snap,
		Camera,
		ShowFlag,
		ViewModeLit,
		ViewModeUnlit,
		ViewModeWireframe,
		ViewModeSceneDepth,
		ViewModeWorldNormal,
		ViewModeLightCulling,
		ViewportPerspective,
		ViewportTop,
		ViewportBottom,
		ViewportLeft,
		ViewportRight,
		ViewportFront,
		ViewportBack,
		ViewportFreeOrtho
	};

	const char* GetPreviewToolbarIconResourceKey(EPreviewToolbarIcon Icon);
	ID3D11ShaderResourceView* FindPreviewToolbarIcon(EPreviewToolbarIcon Icon);
	EPreviewToolbarIcon GetViewModeToolbarIcon(EViewMode ViewMode);
	EPreviewToolbarIcon GetViewportTypeToolbarIcon(ELevelViewportType ViewportType);
	const char* GetViewModeName(EViewMode ViewMode);
	const char* GetViewportTypeName(ELevelViewportType ViewportType);
	void ShowItemTooltip(const char* Tooltip);
	bool DrawIconButton(const char* Id, EPreviewToolbarIcon Icon, const char* Tooltip, bool bSelected = false);
	bool DrawIconTextButton(const char* Id, EPreviewToolbarIcon Icon, const char* Label, const char* Tooltip, float Width);
	void DrawPopupSectionHeader(const char* Label);
	void DrawShowFlagsPopupContent(FViewportRenderOptions& Opts);
	void DrawSnapPopupContent(FEditorSettings& Settings);
	void DrawCameraPopupContent(FPreviewViewportClient& Client);
	void DrawViewportTypePopupContent(FPreviewViewportClient& Client);
	void DrawViewModePopupContent(FViewportRenderOptions& Opts);
}

void FPreviewViewportWidget::SetViewportClient(FPreviewViewportClient* InClient)
{
	ViewportClient = InClient;
}

void FPreviewViewportWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!ViewportClient)
	{
		return;
	}

	FViewport* Viewport = ViewportClient->GetViewport();
	if (!Viewport || !Viewport->GetSRV())
	{
		ViewportClient->SetHovered(false);
		ViewportClient->SetActive(false);
		return;
	}

	const float ToolbarHeight = ImGui::GetFrameHeight() + 8.0f;
	if (ImGui::BeginChild("##PreviewViewportToolbar", ImVec2(0.0f, ToolbarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 0.96f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.36f, 1.0f));

		FEditorSettings& Settings = FEditorSettings::Get();
		FViewportRenderOptions& Opts = ViewportClient->GetRenderOptions();
		PreviewGizmoMode = ViewportClient->GetPreviewGizmoMode();
		const float Width = ImGui::GetContentRegionAvail().x;
		const float ButtonSpacing = 4.0f;

		if (DrawIconButton("##PreviewTranslateTool", EPreviewToolbarIcon::Translate, "Translate", PreviewGizmoMode == 0))
		{
			PreviewGizmoMode = 0;
			ViewportClient->SetPreviewGizmoMode(PreviewGizmoMode);
		}
		ImGui::SameLine(0.0f, ButtonSpacing);
		if (DrawIconButton("##PreviewRotateTool", EPreviewToolbarIcon::Rotate, "Rotate", PreviewGizmoMode == 1))
		{
			PreviewGizmoMode = 1;
			ViewportClient->SetPreviewGizmoMode(PreviewGizmoMode);
		}
		ImGui::SameLine(0.0f, ButtonSpacing);
		if (DrawIconButton("##PreviewScaleTool", EPreviewToolbarIcon::Scale, "Scale", PreviewGizmoMode == 2))
		{
			PreviewGizmoMode = 2;
			ViewportClient->SetPreviewGizmoMode(PreviewGizmoMode);
		}
		ImGui::SameLine(0.0f, 10.0f);

		const bool bWorldCoord = Settings.CoordSystem == EEditorCoordSystem::World;
		if (DrawIconButton("##PreviewCoordSystem", bWorldCoord ? EPreviewToolbarIcon::WorldSpace : EPreviewToolbarIcon::LocalSpace, bWorldCoord ? "World Space" : "Local Space", bWorldCoord))
		{
			Settings.CoordSystem = bWorldCoord ? EEditorCoordSystem::Local : EEditorCoordSystem::World;
		}
		ImGui::SameLine(0.0f, ButtonSpacing);
		const bool bAnySnapEnabled = Settings.bEnableTranslationSnap || Settings.bEnableRotationSnap || Settings.bEnableScaleSnap;
		if (DrawIconButton("##PreviewSnapSettings", EPreviewToolbarIcon::Snap, "Snap Settings", bAnySnapEnabled))
		{
			ImGui::OpenPopup("PreviewSnapPopup");
		}

		const float CameraButtonWidth = 34.0f;
		const float ViewportButtonWidth = Width >= 620.0f ? 136.0f : 34.0f;
		const float ViewModeButtonWidth = Width >= 760.0f ? 136.0f : 34.0f;
		const float ShowButtonWidth = 34.0f;
		const float RightWidth = CameraButtonWidth + ViewportButtonWidth + ViewModeButtonWidth + ShowButtonWidth + ButtonSpacing * 3.0f;
		const float RightStartX = (std::max)(ImGui::GetCursorPosX() + 12.0f, ImGui::GetWindowWidth() - RightWidth - 4.0f);
		ImGui::SameLine(RightStartX, 0.0f);

		if (DrawIconButton("##PreviewCameraSettings", EPreviewToolbarIcon::Camera, "Camera Settings"))
		{
			ImGui::OpenPopup("PreviewCameraPopup");
		}
		ImGui::SameLine(0.0f, ButtonSpacing);
		const bool bShowViewportLabel = Width >= 620.0f;
		if (bShowViewportLabel
			? DrawIconTextButton("##PreviewViewportType", GetViewportTypeToolbarIcon(Opts.ViewportType), GetViewportTypeName(Opts.ViewportType), "Viewport Type", ViewportButtonWidth)
			: DrawIconButton("##PreviewViewportType", GetViewportTypeToolbarIcon(Opts.ViewportType), "Viewport Type"))
		{
			ImGui::OpenPopup("PreviewViewportPopup");
		}
		ImGui::SameLine(0.0f, ButtonSpacing);
		const bool bShowViewModeLabel = Width >= 760.0f;
		if (bShowViewModeLabel
			? DrawIconTextButton("##PreviewViewMode", GetViewModeToolbarIcon(Opts.ViewMode), GetViewModeName(Opts.ViewMode), "View Mode", ViewModeButtonWidth)
			: DrawIconButton("##PreviewViewMode", GetViewModeToolbarIcon(Opts.ViewMode), "View Mode"))
		{
			ImGui::OpenPopup("PreviewViewModePopup");
		}
		ImGui::SameLine(0.0f, ButtonSpacing);
		if (DrawIconButton("##PreviewShowFlags", EPreviewToolbarIcon::ShowFlag, "Show"))
		{
			ImGui::OpenPopup("PreviewShowPopup");
		}

		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.13f, 0.15f, 0.98f));
		if (ImGui::BeginPopup("PreviewSnapPopup"))
		{
			DrawSnapPopupContent(Settings);
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("PreviewCameraPopup"))
		{
			DrawCameraPopupContent(*ViewportClient);
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("PreviewViewportPopup"))
		{
			DrawViewportTypePopupContent(*ViewportClient);
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("PreviewViewModePopup"))
		{
			DrawViewModePopupContent(Opts);
			ImGui::EndPopup();
		}
		ImGui::SetNextWindowSize(ImVec2(286.0f, 0.0f), ImGuiCond_Appearing);
		if (ImGui::BeginPopup("PreviewShowPopup"))
		{
			DrawShowFlagsPopupContent(Opts);
			ImGui::EndPopup();
		}
		ImGui::PopStyleColor();

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
	}
	ImGui::EndChild();

	ImVec2 Size = ImGui::GetContentRegionAvail();
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		ViewportClient->SetHovered(false);
		ViewportClient->SetActive(false);
		return;
	}

	const uint32 NewWidth = static_cast<uint32>(Size.x);
	const uint32 NewHeight = static_cast<uint32>(Size.y);
	if (NewWidth != Viewport->GetWidth() || NewHeight != Viewport->GetHeight())
	{
		Viewport->RequestResize(NewWidth, NewHeight);
	}

	ImGui::Image((ImTextureID)Viewport->GetSRV(), Size);

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bIsHovered = ImGui::IsItemHovered();
	const bool bIsActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	ViewportClient->SetViewportRect(Min.x, Min.y, Max.x - Min.x, Max.y - Min.y);
	ViewportClient->SetHovered(bIsHovered);
	ViewportClient->SetActive(bIsActive);
}

namespace
{
	const char* GetPreviewToolbarIconResourceKey(EPreviewToolbarIcon Icon)
	{
		switch (Icon)
		{
		case EPreviewToolbarIcon::Translate: return "Editor.ToolIcon.Translate";
		case EPreviewToolbarIcon::Rotate: return "Editor.ToolIcon.Rotate";
		case EPreviewToolbarIcon::Scale: return "Editor.ToolIcon.Scale";
		case EPreviewToolbarIcon::WorldSpace: return "Editor.ToolIcon.WorldSpace";
		case EPreviewToolbarIcon::LocalSpace: return "Editor.ToolIcon.LocalSpace";
		case EPreviewToolbarIcon::Snap: return "Editor.ToolIcon.TranslateSnap";
		case EPreviewToolbarIcon::Camera: return "Editor.ToolIcon.Camera";
		case EPreviewToolbarIcon::ShowFlag: return "Editor.ToolIcon.ShowFlag";
		case EPreviewToolbarIcon::ViewModeLit: return "Editor.ToolIcon.ViewMode.Lit";
		case EPreviewToolbarIcon::ViewModeUnlit: return "Editor.ToolIcon.ViewMode.Unlit";
		case EPreviewToolbarIcon::ViewModeWireframe: return "Editor.ToolIcon.ViewMode.Wireframe";
		case EPreviewToolbarIcon::ViewModeSceneDepth: return "Editor.ToolIcon.ViewMode.SceneDepth";
		case EPreviewToolbarIcon::ViewModeWorldNormal: return "Editor.ToolIcon.ViewMode.WorldNormal";
		case EPreviewToolbarIcon::ViewModeLightCulling: return "Editor.ToolIcon.ViewMode.LightCulling";
		case EPreviewToolbarIcon::ViewportPerspective: return "Editor.ToolIcon.Viewport.Perspective";
		case EPreviewToolbarIcon::ViewportTop: return "Editor.ToolIcon.Viewport.Top";
		case EPreviewToolbarIcon::ViewportBottom: return "Editor.ToolIcon.Viewport.Bottom";
		case EPreviewToolbarIcon::ViewportLeft: return "Editor.ToolIcon.Viewport.Left";
		case EPreviewToolbarIcon::ViewportRight: return "Editor.ToolIcon.Viewport.Right";
		case EPreviewToolbarIcon::ViewportFront: return "Editor.ToolIcon.Viewport.Front";
		case EPreviewToolbarIcon::ViewportBack: return "Editor.ToolIcon.Viewport.Back";
		case EPreviewToolbarIcon::ViewportFreeOrtho: return "Editor.ToolIcon.Viewport.FreeOrtho";
		default: return "";
		}
	}

	ID3D11ShaderResourceView* FindPreviewToolbarIcon(EPreviewToolbarIcon Icon)
	{
		const FString Path = FResourceManager::Get().ResolvePath(FName(GetPreviewToolbarIconResourceKey(Icon)));
		return FResourceManager::Get().FindLoadedTexture(Path).Get();
	}

	EPreviewToolbarIcon GetViewModeToolbarIcon(EViewMode ViewMode)
	{
		switch (ViewMode)
		{
		case EViewMode::Unlit: return EPreviewToolbarIcon::ViewModeUnlit;
		case EViewMode::Wireframe: return EPreviewToolbarIcon::ViewModeWireframe;
		case EViewMode::SceneDepth: return EPreviewToolbarIcon::ViewModeSceneDepth;
		case EViewMode::WorldNormal: return EPreviewToolbarIcon::ViewModeWorldNormal;
		case EViewMode::LightCulling: return EPreviewToolbarIcon::ViewModeLightCulling;
		default: return EPreviewToolbarIcon::ViewModeLit;
		}
	}

	EPreviewToolbarIcon GetViewportTypeToolbarIcon(ELevelViewportType ViewportType)
	{
		switch (ViewportType)
		{
		case ELevelViewportType::Top: return EPreviewToolbarIcon::ViewportTop;
		case ELevelViewportType::Bottom: return EPreviewToolbarIcon::ViewportBottom;
		case ELevelViewportType::Left: return EPreviewToolbarIcon::ViewportLeft;
		case ELevelViewportType::Right: return EPreviewToolbarIcon::ViewportRight;
		case ELevelViewportType::Front: return EPreviewToolbarIcon::ViewportFront;
		case ELevelViewportType::Back: return EPreviewToolbarIcon::ViewportBack;
		case ELevelViewportType::FreeOrthographic: return EPreviewToolbarIcon::ViewportFreeOrtho;
		default: return EPreviewToolbarIcon::ViewportPerspective;
		}
	}

	const char* GetViewModeName(EViewMode ViewMode)
	{
		switch (ViewMode)
		{
		case EViewMode::Unlit: return "Unlit";
		case EViewMode::Lit_Gouraud: return "Gouraud";
		case EViewMode::Lit_Lambert: return "Lambert";
		case EViewMode::Wireframe: return "Wireframe";
		case EViewMode::SceneDepth: return "Scene Depth";
		case EViewMode::WorldNormal: return "World Normal";
		case EViewMode::LightCulling: return "Light Culling";
		default: return "Lit";
		}
	}

	const char* GetViewportTypeName(ELevelViewportType ViewportType)
	{
		switch (ViewportType)
		{
		case ELevelViewportType::Top: return "Top";
		case ELevelViewportType::Bottom: return "Bottom";
		case ELevelViewportType::Left: return "Left";
		case ELevelViewportType::Right: return "Right";
		case ELevelViewportType::Front: return "Front";
		case ELevelViewportType::Back: return "Back";
		case ELevelViewportType::FreeOrthographic: return "Free Ortho";
		default: return "Perspective";
		}
	}

	void ShowItemTooltip(const char* Tooltip)
	{
		if (Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(Tooltip);
			ImGui::EndTooltip();
		}
	}

	bool DrawIconButton(const char* Id, EPreviewToolbarIcon Icon, const char* Tooltip, bool bSelected)
	{
		if (bSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, EditorAccentColor::Value);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorAccentColor::Value);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorAccentColor::Value);
		}

		bool bClicked = false;
		if (ID3D11ShaderResourceView* IconSRV = FindPreviewToolbarIcon(Icon))
		{
			bClicked = ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(IconSRV), ImVec2(18.0f, 18.0f), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec4(0, 0, 0, 0));
		}
		else
		{
			bClicked = ImGui::Button(Tooltip);
		}

		if (bSelected)
		{
			ImGui::PopStyleColor(3);
		}
		ShowItemTooltip(Tooltip);
		return bClicked;
	}

	bool DrawIconTextButton(const char* Id, EPreviewToolbarIcon Icon, const char* Label, const char* Tooltip, float Width)
	{
		const bool bClicked = ImGui::Button(Id, ImVec2(Width, 28.0f));
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		if (ID3D11ShaderResourceView* IconSRV = FindPreviewToolbarIcon(Icon))
		{
			const float IconY = Min.y + (Max.y - Min.y - 18.0f) * 0.5f;
			DrawList->AddImage(reinterpret_cast<ImTextureID>(IconSRV), ImVec2(Min.x + 7.0f, IconY), ImVec2(Min.x + 25.0f, IconY + 18.0f));
		}
		DrawList->AddText(ImVec2(Min.x + 31.0f, Min.y + (Max.y - Min.y - ImGui::GetTextLineHeight()) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), Label);
		ShowItemTooltip(Tooltip);
		return bClicked;
	}

	void DrawPopupSectionHeader(const char* Label)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.84f, 1.0f));
		ImGui::SeparatorText(Label);
		ImGui::PopStyleColor();
	}

	void DrawShowFlagsPopupContent(FViewportRenderOptions& Opts)
	{
		constexpr float SliderWidth = 150.0f;
		DrawPopupSectionHeader("COMMON SHOW FLAGS");
		ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
		ImGui::Checkbox("Billboard Text", &Opts.ShowFlags.bBillboardText);

		DrawPopupSectionHeader("ACTOR HELPERS");
		ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);
		if (Opts.ShowFlags.bGrid)
		{
			ImGui::SetNextItemWidth(SliderWidth);
			ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
			ImGui::SetNextItemWidth(SliderWidth);
			ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);
		}
		ImGui::Checkbox("World Axis", &Opts.ShowFlags.bWorldAxis);
		ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);

		DrawPopupSectionHeader("DEBUG");
		ImGui::Checkbox("Debug Draw", &Opts.ShowFlags.bDebugDraw);
		ImGui::Checkbox("Bounding Volume", &Opts.ShowFlags.bBoundingVolume);
		ImGui::Checkbox("Light Visualization", &Opts.ShowFlags.bLightVisualization);
		ImGui::Checkbox("Light Hit Map", &Opts.ShowFlags.bLightHitMap);
		ImGui::Checkbox("Shadow Frustum", &Opts.ShowFlags.bShowShadowFrustum);

		DrawPopupSectionHeader("POST-PROCESSING");
		ImGui::Checkbox("Height Distance Fog", &Opts.ShowFlags.bFog);
		ImGui::Checkbox("Anti-Aliasing (FXAA)", &Opts.ShowFlags.bFXAA);
		ImGui::Checkbox("Gamma Correction", &Opts.ShowFlags.bGammaCorrection);
		if (Opts.ShowFlags.bGammaCorrection)
		{
			ImGui::SetNextItemWidth(SliderWidth);
			ImGui::SliderFloat("Display Gamma", &Opts.DisplayGamma, 1.0f, 3.0f, "%.2f");
			ImGui::SetNextItemWidth(SliderWidth);
			ImGui::SliderFloat("Gamma Blend", &Opts.GammaCorrectionBlend, 0.0f, 1.0f, "%.2f");
		}
	}

	void DrawSnapOptionGroup(const char* Label, bool& bEnabled, float& Value, const float* Options, int32 Count, const char* Format)
	{
		ImGui::PushID(Label);
		ImGui::Checkbox(Label, &bEnabled);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(86.0f);
		if (ImGui::InputFloat("##Value", &Value, 0.0f, 0.0f, "%.5g"))
		{
			Value = (std::max)(Value, 0.00001f);
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			char ChoiceLabel[32];
			snprintf(ChoiceLabel, sizeof(ChoiceLabel), Format, Options[Index]);
			if (Index > 0)
			{
				ImGui::SameLine();
			}
			if (ImGui::SmallButton(ChoiceLabel))
			{
				Value = Options[Index];
			}
		}
		ImGui::PopID();
	}

	void DrawSnapPopupContent(FEditorSettings& Settings)
	{
		static const float TranslationSnapSizes[] = { 1.0f, 5.0f, 10.0f, 50.0f, 100.0f };
		static const float RotationSnapSizes[] = { 5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f };
		static const float ScaleSnapSizes[] = { 0.03125f, 0.0625f, 0.1f, 0.25f, 0.5f, 1.0f };
		DrawPopupSectionHeader("SNAPPING");
		DrawSnapOptionGroup("Location", Settings.bEnableTranslationSnap, Settings.TranslationSnapSize, TranslationSnapSizes, 5, "%.0f");
		DrawSnapOptionGroup("Rotation", Settings.bEnableRotationSnap, Settings.RotationSnapSize, RotationSnapSizes, 6, "%.0f");
		DrawSnapOptionGroup("Scale", Settings.bEnableScaleSnap, Settings.ScaleSnapSize, ScaleSnapSizes, 6, "%.5g");
	}

	void DrawCameraPopupContent(FPreviewViewportClient& Client)
	{
		DrawPopupSectionHeader("CAMERA");
		float Speed = Client.GetPreviewCameraSpeed();
		if (ImGui::DragFloat("Speed", &Speed, 0.1f, 0.1f, 1000.0f, "%.1f"))
		{
			Client.SetPreviewCameraSpeed(Speed);
		}

		UCameraComponent* Camera = Client.GetCamera();
		if (!Camera)
		{
			return;
		}

		float FOV = Camera->GetFOV() * RAD_TO_DEG;
		if (ImGui::DragFloat("FOV", &FOV, 0.5f, 1.0f, 170.0f, "%.1f"))
		{
			Camera->SetFOV(Clamp(FOV, 1.0f, 170.0f) * DEG_TO_RAD);
		}

		float OrthoWidth = Camera->GetOrthoWidth();
		if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 100000.0f, "%.1f"))
		{
			Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 100000.0f));
		}

		FVector Location = Camera->GetWorldLocation();
		float LocationValues[3] = { Location.X, Location.Y, Location.Z };
		if (ImGui::DragFloat3("Location", LocationValues, 0.1f))
		{
			Camera->SetWorldLocation(FVector(LocationValues[0], LocationValues[1], LocationValues[2]));
		}

		FRotator Rotation = Camera->GetRelativeRotation();
		float RotationValues[3] = { Rotation.Roll, Rotation.Pitch, Rotation.Yaw };
		if (ImGui::DragFloat3("Rotation", RotationValues, 0.1f))
		{
			Camera->SetRelativeRotation(FRotator(RotationValues[1], RotationValues[2], RotationValues[0]));
		}
	}

	void DrawViewportTypePopupContent(FPreviewViewportClient& Client)
	{
		DrawPopupSectionHeader("PERSPECTIVE");
		if (ImGui::Selectable("Perspective", Client.GetRenderOptions().ViewportType == ELevelViewportType::Perspective))
		{
			Client.SetViewportType(ELevelViewportType::Perspective);
		}
		DrawPopupSectionHeader("ORTHOGRAPHIC");
		for (int32 TypeIndex = static_cast<int32>(ELevelViewportType::Top); TypeIndex <= static_cast<int32>(ELevelViewportType::FreeOrthographic); ++TypeIndex)
		{
			const ELevelViewportType Type = static_cast<ELevelViewportType>(TypeIndex);
			if (ImGui::Selectable(GetViewportTypeName(Type), Client.GetRenderOptions().ViewportType == Type))
			{
				Client.SetViewportType(Type);
			}
		}
	}

	void DrawViewModePopupContent(FViewportRenderOptions& Opts)
	{
		DrawPopupSectionHeader("VIEW MODE");
		for (int32 ModeIndex = 0; ModeIndex < static_cast<int32>(EViewMode::Count); ++ModeIndex)
		{
			const EViewMode Mode = static_cast<EViewMode>(ModeIndex);
			if (ImGui::Selectable(GetViewModeName(Mode), Opts.ViewMode == Mode))
			{
				Opts.ViewMode = Mode;
			}
		}
	}
}
