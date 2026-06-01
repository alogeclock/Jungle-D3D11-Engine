#include "ViewportToolbar.h"
#include "Component/GizmoComponent.h"
#include "Editor/UI/EditorTextureManager.h"
#include "Render/Pipeline/Renderer.h"
#include "Platform/Paths.h"
#include "Settings/EditorViewportSettings.h"
#include "Settings/GizmoToolSettings.h"

#include "ImGui/imgui.h"

namespace
{
	constexpr float ToolbarDefaultHeight = 28.0f;
	constexpr float ToolbarIconSize = 16.0f;
	constexpr float ToolbarIconButtonWidth = 24.0f;
	constexpr float ToolbarTextButtonWidth = 90.0f;
	constexpr float ToolbarButtonSpacing = 4.0f;
	constexpr float ToolbarGroupSpacing = 12.0f;
	constexpr float ToolbarPlayStopButtonWidth = 24.0f;

	constexpr ImVec4 ToolbarPopupBgColor = ImVec4(0.22f, 0.22f, 0.22f, 0.98f);
	constexpr ImVec4 ToolbarPopupBorderColor = ImVec4(0.34f, 0.34f, 0.34f, 1.0f);
	constexpr ImVec4 ToolbarPopupFrameBgColor = ImVec4(0.29f, 0.29f, 0.29f, 1.0f);
	constexpr ImVec4 ToolbarPopupFrameBgHoveredColor = ImVec4(0.36f, 0.36f, 0.36f, 1.0f);
	constexpr ImVec4 ToolbarPopupFrameBgActiveColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
	constexpr ImVec4 ToolbarPopupButtonColor = ImVec4(0.29f, 0.29f, 0.29f, 1.0f);
	constexpr ImVec4 ToolbarPopupButtonHoveredColor = ImVec4(0.38f, 0.38f, 0.38f, 1.0f);
	constexpr ImVec4 ToolbarPopupButtonActiveColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
	constexpr ImVec4 ToolbarPopupHeaderColor = ImVec4(0.0f, 0.71f, 0.86f, 0.22f);
	constexpr ImVec4 ToolbarPopupHeaderHoveredColor = ImVec4(0.0f, 0.71f, 0.86f, 0.32f);
	constexpr ImVec4 ToolbarPopupHeaderActiveColor = ImVec4(0.0f, 0.71f, 0.86f, 0.42f);
	constexpr ImVec4 ToolbarPopupCheckMarkColor = ImVec4(0.0f, 0.78f, 0.95f, 1.0f);
	constexpr ImVec2 ToolbarPopupWindowPadding = ImVec2(14.0f, 10.0f);
	constexpr ImVec2 ToolbarPopupItemSpacing = ImVec2(8.0f, 8.0f);
	constexpr ImVec2 ToolbarPopupFramePadding = ImVec2(6.0f, 5.0f);

	float GetToolbarHeight(const FViewportToolbarContext& Context)
	{
		return Context.ToolbarHeight > 0.0f ? Context.ToolbarHeight : ToolbarDefaultHeight;
	}

	float GetToolbarButtonPadding(const FViewportToolbarContext& Context)
	{
		return (GetToolbarHeight(Context) - ToolbarIconSize) * 0.5f;
	}

	void PushToolbarPopupStyle()
	{
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ToolbarPopupBgColor);
		ImGui::PushStyleColor(ImGuiCol_Border, ToolbarPopupBorderColor);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ToolbarPopupFrameBgColor);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToolbarPopupFrameBgHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ToolbarPopupFrameBgActiveColor);
		ImGui::PushStyleColor(ImGuiCol_Button, ToolbarPopupButtonColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToolbarPopupButtonHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToolbarPopupButtonActiveColor);
		ImGui::PushStyleColor(ImGuiCol_Header, ToolbarPopupHeaderColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToolbarPopupHeaderHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ToolbarPopupHeaderActiveColor);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, ToolbarPopupCheckMarkColor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ToolbarPopupWindowPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ToolbarPopupItemSpacing);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ToolbarPopupFramePadding);
	}

	void PopToolbarPopupStyle()
	{
		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor(12);
	}
}

#pragma region Toolbar Icon Helper
static FString GetToolbarIconPath(EToolbarIcon Icon)
{
	const wchar_t* FileName = L"";
	switch (Icon)
	{
	case EToolbarIcon::Menu: FileName = L"Menu.png"; break;
	case EToolbarIcon::Setting: FileName = L"Setting.png"; break;
	case EToolbarIcon::AddActor: FileName = L"Add_Actor.png"; break;
	case EToolbarIcon::Translate: FileName = L"Translate.png"; break;
	case EToolbarIcon::Rotate: FileName = L"Rotate.png"; break;
	case EToolbarIcon::Scale: FileName = L"Scale.png"; break;
	case EToolbarIcon::WorldSpace: FileName = L"WorldSpace.png"; break;
	case EToolbarIcon::LocalSpace: FileName = L"LocalSpace.png"; break;
	case EToolbarIcon::TranslateSnap: FileName = L"Translate_Snap.png"; break;
	case EToolbarIcon::RotateSnap: FileName = L"Rotate_Snap.png"; break;
	case EToolbarIcon::ScaleSnap: FileName = L"Scale_Snap.png"; break;
	case EToolbarIcon::ShowFlag: FileName = L"Show_Flag.png"; break;
	case EToolbarIcon::Camera: FileName = L"Camera.png"; break;
	default: break;
	}

	return FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/", FileName));
}

static ImVec2 GetToolbarIconRenderSize(EToolbarIcon Icon, float FallbackSize, float MaxIconSize)
{
	ID3D11ShaderResourceView* IconSRV = FEditorTextureManager::Get().GetOrLoadIcon(GetToolbarIconPath(Icon));
	if (!IconSRV)
	{
		return ImVec2(FallbackSize, FallbackSize);
	}

	ID3D11Resource* Resource = nullptr;
	IconSRV->GetResource(&Resource);
	if (!Resource)
	{
		return ImVec2(FallbackSize, FallbackSize);
	}

	ImVec2 IconSize(FallbackSize, FallbackSize);
	D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	Resource->GetType(&Dimension);
	if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
	{
		ID3D11Texture2D* Texture2D = static_cast<ID3D11Texture2D*>(Resource);
		D3D11_TEXTURE2D_DESC Desc = {};
		Texture2D->GetDesc(&Desc);
		IconSize = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
	}
	Resource->Release();

	if (IconSize.x > MaxIconSize || IconSize.y > MaxIconSize)
	{
		const float Scale = (IconSize.x > IconSize.y) ? (MaxIconSize / IconSize.x) : (MaxIconSize / IconSize.y);
		IconSize.x *= Scale;
		IconSize.y *= Scale;
	}

	return IconSize;
}

static bool DrawToolbarIconButton(const char* Id, EToolbarIcon Icon, const char* FallbackLabel, float FallbackSize, float MaxIconSize)
{
	ID3D11ShaderResourceView* IconSRV = FEditorTextureManager::Get().GetOrLoadIcon(GetToolbarIconPath(Icon));
	if (!IconSRV)
	{
		return ImGui::Button(FallbackLabel);
	}

	const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackSize, MaxIconSize);
	return ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(IconSRV), IconSize);
}
#pragma endregion

#pragma region LeftRight Section Helper

static float CalcRightToolbarWidth(const FViewportToolbarContext& Context)
{
	float Width = 0.0f;
	const float CameraValueWidth = ImGui::CalcTextSize("100.000").x;

	auto AddGroup = [&](float ItemWidth)
	{
		if (Width > 0.0f)
		{
			Width += ToolbarGroupSpacing;
		}
		Width += ItemWidth;
	};

	if (Context.bShowViewportType)
	{
		AddGroup(ToolbarTextButtonWidth);
	}

	if (Context.bShowCameraControls)
	{
		AddGroup(ToolbarIconButtonWidth + 2.0f + CameraValueWidth);
	}

	if (Context.bShowViewMode)
	{
		AddGroup(ToolbarIconButtonWidth);
	}

	if (Context.bShowShowFlags)
	{
		AddGroup(ToolbarIconButtonWidth);
	}

	if (Context.bShowLayoutControls)
	{
		AddGroup(ToolbarIconButtonWidth * 2 + ToolbarButtonSpacing);
	}

	return Width;
}

static void SameLineIfNeeded(bool& bHasItem, float Spacing)
{
	if (bHasItem)
	{
		ImGui::SameLine(0, Spacing);
	}
	bHasItem = true;
}

#pragma endregion

void FViewportToolbar::Render(const FViewportToolbarContext& Context)
{
	if (!Context.Settings) return;

	FToolbarRenderState RenderState(Context);

	ImGui::PushID(static_cast<int32>(Context.ToolbarLeft));
	ImGui::PushID(static_cast<int32>(Context.ToolbarTop));

	BeginToolbar(RenderState);

	RenderLeftToolbarSection(RenderState);

	const float RightWidth = CalcRightToolbarWidth(Context);
	const float ButtonPadding = GetToolbarButtonPadding(Context);
	const float RightX = Context.ToolbarLeft + Context.ToolbarWidth - RightWidth - ButtonPadding;

	ImGui::SetCursorScreenPos(ImVec2(RightX, Context.ToolbarTop + ButtonPadding));

	RenderRightToolbarSection(RenderState);

	EndToolbar(RenderState);

	ImGui::PopID();
	ImGui::PopID();

	if (Context.OnSettingsChanged)
	{
		Context.OnSettingsChanged();
	}
}

void FViewportToolbar::BeginToolbar(const FToolbarRenderState& State)
{
	const float ButtonPadding = GetToolbarButtonPadding(State.Context);

	const float PlayStopOffset = State.Context.bReservePlayStopSpace ? (ToolbarPlayStopButtonWidth * 2.0f) + ButtonPadding : 0.0f;

	ImGui::SetCursorScreenPos(ImVec2(
		State.Context.ToolbarLeft + ButtonPadding + PlayStopOffset,
		State.Context.ToolbarTop + ButtonPadding));

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.3f));
}

void FViewportToolbar::EndToolbar(const FToolbarRenderState& State)
{
	ImGui::PopStyleColor(3);
}

void FViewportToolbar::RenderLeftToolbarSection(const FToolbarRenderState& State)
{
	if (State.Context.bShowAddActor)
	{
		RenderAddActor(State);
	}

	if (State.Context.bShowGizmoControls)
	{
		RenderGizmoControls(State);
	}
}

void FViewportToolbar::RenderRightToolbarSection(const FToolbarRenderState& State)
{
	bool bHasItem = false;

	if (State.Context.bShowViewportType)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderViewportType(State);
	}

	if (State.Context.bShowCameraControls)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderCameraControls(State);
	}

	if (State.Context.bShowViewMode)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderViewMode(State);
	}

	if (State.Context.bShowShowFlags)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderShowFlags(State);
	}

	if (State.Context.bShowLayoutControls)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderLayoutControls(State);
	}
}

void FViewportToolbar::RenderLayoutControls(const FToolbarRenderState& State)
{
	if (DrawToolbarIconButton("##Layout", EToolbarIcon::Menu, "Layout",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##LayoutPopup");
	}

	PushToolbarPopupStyle();
	if (ImGui::BeginPopup("##LayoutPopup"))
	{
		constexpr int32 Columns = 4;
		constexpr float IconSize = 32.0f;
		const int32 LayoutCount = State.Context.LayoutIconCount;
		for (int32 LayoutIndex = 0; LayoutIndex < LayoutCount; ++LayoutIndex)
		{
			ImGui::PushID(LayoutIndex);

			const bool bSelected = LayoutIndex == State.Context.CurrentLayoutIndex;
			if (bSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
			}

			bool bClicked = false;
			ID3D11ShaderResourceView* LayoutIcon =
				State.Context.LayoutIcons ? State.Context.LayoutIcons[LayoutIndex] : nullptr;
			if (LayoutIcon)
			{
				bClicked = ImGui::ImageButton("##LayoutIcon",
					reinterpret_cast<ImTextureID>(LayoutIcon),
					ImVec2(IconSize, IconSize));
			}
			else
			{
				char FallbackLabel[8];
				snprintf(FallbackLabel, sizeof(FallbackLabel), "%d", LayoutIndex);
				bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize + 8.0f, IconSize + 8.0f));
			}

			if (bSelected)
			{
				ImGui::PopStyleColor();
			}

			if (bClicked)
			{
				if (State.Context.OnLayoutSelected)
				{
					State.Context.OnLayoutSelected(LayoutIndex);
				}
				ImGui::CloseCurrentPopup();
			}

			if ((LayoutIndex + 1) % Columns != 0 && LayoutIndex + 1 < LayoutCount)
			{
				ImGui::SameLine();
			}

			ImGui::PopID();
		}
		ImGui::EndPopup();
	}
	PopToolbarPopupStyle();

	ImGui::SameLine(0.0f, State.ButtonSpacing);

	bool bToggleClicked = false;
	ID3D11ShaderResourceView* ToggleIcon = nullptr;
	if (State.Context.LayoutIcons &&
		State.Context.ToggleLayoutIndex >= 0 &&
		State.Context.ToggleLayoutIndex < State.Context.LayoutIconCount)
	{
		ToggleIcon = State.Context.LayoutIcons[State.Context.ToggleLayoutIndex];
	}

	if (ToggleIcon)
	{
		bToggleClicked = ImGui::ImageButton("##ToggleLayout",
			reinterpret_cast<ImTextureID>(ToggleIcon),
			ImVec2(State.MaxIconSize, State.MaxIconSize));
	}
	else
	{
		bToggleClicked = DrawToolbarIconButton("##ToggleLayout", EToolbarIcon::Menu, "Toggle",
			State.FallbackIconSize, State.MaxIconSize);
	}

	if (bToggleClicked)
	{
		if (State.Context.OnToggleLayout)
		{
			State.Context.OnToggleLayout();
		}
	}
}

void FViewportToolbar::RenderAddActor(const FToolbarRenderState& State)
{
	if (DrawToolbarIconButton("###SharedAddActorIcon", EToolbarIcon::AddActor, "Add Actor",
		State.FallbackIconSize, State.MaxIconSize))
	{
		if (State.Context.OnAddActorClicked)
		{
			State.Context.OnAddActorClicked();
		}
	}

	ImGui::SameLine(0, State.GroupSpacing);
}

void FViewportToolbar::RenderViewportType(const FToolbarRenderState& State)
{
	FViewportRenderOptions& RenderOptions = State.RenderOptions();

	static const char* ViewportTypeNames[] = {
		"Perspective",
		"Top",
		"Bottom",
		"Left",
		"Right",
		"Front",
		"Back",
		"Free Orthographic"
	};

	constexpr int32 ViewportTypeCount = sizeof(ViewportTypeNames) / sizeof(ViewportTypeNames[0]);
	int32 CurrentTypeIndex = static_cast<int32>(RenderOptions.ViewportType);
	if (CurrentTypeIndex < 0 || CurrentTypeIndex >= ViewportTypeCount)
	{
		CurrentTypeIndex = 0;
	}

	ImGui::SetNextItemWidth(90.0f);
	if (ImGui::Button(ViewportTypeNames[CurrentTypeIndex], ImVec2(90.0f, 0.0f)))
	{
		ImGui::OpenPopup("##ViewportTypePopup");
	}

	PushToolbarPopupStyle();
	if (ImGui::BeginPopup("##ViewportTypePopup"))
	{
		for (int32 TypeIndex = 0; TypeIndex < ViewportTypeCount; ++TypeIndex)
		{
			const bool bSelected = TypeIndex == CurrentTypeIndex;
			if (ImGui::Selectable(ViewportTypeNames[TypeIndex], bSelected))
			{
				const ELevelViewportType NewType = static_cast<ELevelViewportType>(TypeIndex);
				if (State.Context.OnViewportTypeSelected)
				{
					State.Context.OnViewportTypeSelected(NewType);
				}
				else
				{
					RenderOptions.ViewportType = NewType;
				}
			}
		}
		ImGui::EndPopup();
	}
	PopToolbarPopupStyle();
}

void FViewportToolbar::RenderGizmoControls(const FToolbarRenderState& State)
{
	UGizmoComponent* Gizmo = State.Context.Gizmo;
	if (!Gizmo) return;

	auto DrawGizmoIcon = [&](const char* Id, EToolbarIcon Icon, EGizmoMode TargetMode, const char* FallbackLabel) -> bool
	{
		const bool bSelected = (Gizmo->GetMode() == TargetMode);
		if (bSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		}
		const bool bClicked = DrawToolbarIconButton(Id, Icon, FallbackLabel, State.FallbackIconSize, State.MaxIconSize);
		if (bSelected)
		{
			ImGui::PopStyleColor();
		}
		return bClicked;
	};

	if (DrawGizmoIcon("###TranslateIcon", EToolbarIcon::Translate, EGizmoMode::Translate, "Translate"))
	{
		Gizmo->SetTranslateMode();
	}

	ImGui::SameLine(0, State.ButtonSpacing);
	if (DrawGizmoIcon("###RotateIcon", EToolbarIcon::Rotate, EGizmoMode::Rotate, "Rotate"))
	{
		Gizmo->SetRotateMode();
	}

	ImGui::SameLine(0, State.ButtonSpacing);
	if (DrawGizmoIcon("###ScaleIcon", EToolbarIcon::Scale, EGizmoMode::Scale, "Scale"))
	{
		Gizmo->SetScaleMode();
	}

	ImGui::SameLine(0, State.GroupSpacing);

	RenderCoordSystemButton(State);
	RenderSnapControls(State);
}

void FViewportToolbar::RenderCoordSystemButton(const FToolbarRenderState& State)
{
	FGizmoToolSettings& Settings = State.GizmoSettings();

	const bool bWorldCoord = Settings.CoordSystem == EEditorCoordSystem::World;
	if (bWorldCoord)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
	}

	if (DrawToolbarIconButton("###WorldSpaceIcon",
		bWorldCoord ? EToolbarIcon::WorldSpace : EToolbarIcon::LocalSpace,
		bWorldCoord ? "World" : "Local",
		State.FallbackIconSize,
		State.MaxIconSize))
	{
		if (State.Context.OnCoordSystemToggled)
		{
			State.Context.OnCoordSystemToggled();
		}
	}

	if (bWorldCoord)
	{
		ImGui::PopStyleColor();
	}
}

void FViewportToolbar::RenderSnapControls(const FToolbarRenderState& State)
{
	FGizmoToolSettings& Settings = State.GizmoSettings();

	auto DrawSnapControl = [&](const char* Id, EToolbarIcon Icon, const char* FallbackLabel, bool& bEnabled, float& Value, float MinValue)
	{
		ImGui::SameLine(0.0f, 6.0f);
		ImGui::PushID(Id);
		const bool bWasEnabled = bEnabled;
		if (bWasEnabled)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.58f, 0.88f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.42f, 0.72f, 1.0f));
		}
		if (DrawToolbarIconButton("##SnapToggle", Icon, FallbackLabel, State.FallbackIconSize, State.MaxIconSize))
		{
			bEnabled = !bEnabled;
		}
		if (bWasEnabled)
		{
			ImGui::PopStyleColor(3);
		}
		ImGui::SameLine(0.0f, 2.0f);
		ImGui::SetNextItemWidth(48.0f);
		if (ImGui::InputFloat("##Value", &Value, 0.0f, 0.0f, "%.2f") && Value < MinValue)
		{
			Value = MinValue;
		}
		ImGui::PopID();
	};

	DrawSnapControl("TranslateSnap", EToolbarIcon::TranslateSnap, "T", Settings.bEnableTranslationSnap, Settings.TranslationSnapSize, 0.001f);
	DrawSnapControl("RotateSnap", EToolbarIcon::RotateSnap, "R", Settings.bEnableRotationSnap, Settings.RotationSnapSize, 0.001f);
	DrawSnapControl("ScaleSnap", EToolbarIcon::ScaleSnap, "S", Settings.bEnableScaleSnap, Settings.ScaleSnapSize, 0.001f);
}

void FViewportToolbar::RenderCameraControls(const FToolbarRenderState& State)
{
	if (DrawToolbarIconButton("##CameraSettings", EToolbarIcon::Camera, "Camera",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##CameraPopup");
	}

	State.CameraSettings().MoveSpeed;
	ImGui::SameLine(0.0f, 2.0f);
	ImGui::Text("%.3f", State.CameraSettings().MoveSpeed);

	PushToolbarPopupStyle();
	if (ImGui::BeginPopup("##CameraPopup"))
	{
		auto& Camera = State.CameraSettings();

		ImGui::Text("Camera");
		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Move", &Camera.MoveSpeed, 0.1f, 100.0f, "%.1f");

		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Rotation", &Camera.RotationSpeed, 1.0f, 360.0f, "%.1f");

		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Zoom", &Camera.ZoomSpeed, 10.0f, 1000.0f, "%.1f");

		ImGui::EndPopup();
	}
	PopToolbarPopupStyle();
}

void FViewportToolbar::RenderViewMode(const FToolbarRenderState& State)
{
	FViewportRenderOptions& RenderOptions = State.RenderOptions();

	if (DrawToolbarIconButton("##ViewMode", EToolbarIcon::Setting, "View Mode",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##ViewModePopup");
	}

	const ImVec2 ViewModeButtonMin = ImGui::GetItemRectMin();
	const ImVec2 ViewModeButtonMax = ImGui::GetItemRectMax();
	ImGui::SetNextWindowPos(ImVec2(ViewModeButtonMin.x, ViewModeButtonMax.y + 4.0f), ImGuiCond_Appearing);

	PushToolbarPopupStyle();
	if (ImGui::BeginPopup("##ViewModePopup"))
	{
		int32 CurrentMode = static_cast<int32>(RenderOptions.ViewMode);

		ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
		ImGui::RadioButton("Phong", &CurrentMode, static_cast<int32>(EViewMode::Lit_Phong));
		ImGui::RadioButton("Gouraud", &CurrentMode, static_cast<int32>(EViewMode::Lit_Gouraud));
		ImGui::RadioButton("Lambert", &CurrentMode, static_cast<int32>(EViewMode::Lit_Lambert));
		ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
		ImGui::RadioButton("Scene Depth", &CurrentMode, static_cast<int32>(EViewMode::SceneDepth));
		ImGui::RadioButton("World Normal", &CurrentMode, static_cast<int32>(EViewMode::WorldNormal));
		ImGui::RadioButton("Light Culling", &CurrentMode, static_cast<int32>(EViewMode::LightCulling));

		RenderOptions.ViewMode = static_cast<EViewMode>(CurrentMode);

		if (State.Context.OnRenderViewModeExtras)
		{
			ImGui::Separator();
			State.Context.OnRenderViewModeExtras();
		}

		ImGui::EndPopup();
	}
	PopToolbarPopupStyle();
}

void FViewportToolbar::RenderShowFlags(const FToolbarRenderState& State)
{
	FViewportRenderOptions& RenderOptions = State.RenderOptions();

	if (DrawToolbarIconButton("##ShowFlags", EToolbarIcon::ShowFlag, "Show Flags",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##ShowFlagsPopup");
	}

	PushToolbarPopupStyle();
	if (ImGui::BeginPopup("##ShowFlagsPopup"))
	{
		ImGui::Text("Show Flags");

		ImGui::Checkbox("StaticMesh", &RenderOptions.ShowFlags.bStaticMesh);
		ImGui::Checkbox("SkeletalMesh", &RenderOptions.ShowFlags.bSkeletalMesh);
		ImGui::Checkbox("Grid", &RenderOptions.ShowFlags.bGrid);
		ImGui::Checkbox("World Axis", &RenderOptions.ShowFlags.bWorldAxis);
		ImGui::Checkbox("Gizmo", &RenderOptions.ShowFlags.bGizmo);
		ImGui::Checkbox("Billboard Text", &RenderOptions.ShowFlags.bBillboardText);
		ImGui::Checkbox("Bounding Volume", &RenderOptions.ShowFlags.bBoundingVolume);
		ImGui::Checkbox("Debug Draw", &RenderOptions.ShowFlags.bDebugDraw);
		ImGui::Checkbox("Octree", &RenderOptions.ShowFlags.bOctree);
		ImGui::Checkbox("Fog", &RenderOptions.ShowFlags.bFog);
		ImGui::Checkbox("FXAA", &RenderOptions.ShowFlags.bFXAA);
		ImGui::Checkbox("Gamma Correction", &RenderOptions.ShowFlags.bGammaCorrection);
		ImGui::Checkbox("View Light Culling", &RenderOptions.ShowFlags.bViewLightCulling);
		ImGui::Checkbox("Visualize 2.5D Culling", &RenderOptions.ShowFlags.bVisualize25DCulling);
		ImGui::Checkbox("Show Shadow Frustum", &RenderOptions.ShowFlags.bShowShadowFrustum);
		ImGui::Checkbox("Collision", &RenderOptions.ShowFlags.bCollision);
		ImGui::Checkbox("Show Collision Shape", &RenderOptions.ShowFlags.bShowCollisionShape);

		ImGui::EndPopup();
	}
	PopToolbarPopupStyle();
}
