#include "ObjViewer/ObjViewerPanel.h"

#include "ObjViewer/ObjViewerEngine.h"
#include "ObjViewer/ObjViewerViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Input/InputManager.h"
#include "Render/Pipeline/Renderer.h"
#include "Mesh/ObjManager.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "Resource/ResourceManager.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <filesystem>

static constexpr float ObjViewerTitleBarHeight = 42.0f;
static constexpr float ObjViewerOuterPadding = 6.0f;
static constexpr float ObjViewerCornerRadius = 12.0f;
static constexpr float ObjViewerWindowControlButtonWidth = 38.0f;
static constexpr float ObjViewerWindowControlHeight = 24.0f;
static constexpr float ObjViewerWindowControlSpacing = 2.0f;

static const char* GetWindowControlIconMinimize() { return "\xEE\xA4\xA1"; }
static const char* GetWindowControlIconMaximize() { return "\xEE\xA4\xA2"; }
static const char* GetWindowControlIconRestore()  { return "\xEE\xA4\xA3"; }
static const char* GetWindowControlIconClose()    { return "\xEE\xA2\xBB"; }

static void ApplyEditorTabStyle()
{
	ImGuiStyle& Style = ImGui::GetStyle();
	Style.TabRounding = (std::max)(Style.TabRounding, 6.0f);
	Style.TabBorderSize = (std::max)(Style.TabBorderSize, 1.0f);

	Style.Colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
	Style.Colors[ImGuiCol_TabHovered] = ImVec4(0.27f, 0.27f, 0.31f, 1.0f);
	Style.Colors[ImGuiCol_TabSelected] = ImVec4(0.24f, 0.24f, 0.27f, 1.0f);
	Style.Colors[ImGuiCol_TabDimmed] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);
	Style.Colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.22f, 0.22f, 0.25f, 1.0f);
}

static void ApplyEditorColorTheme()
{
	ImGuiStyle& Style = ImGui::GetStyle();
	Style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.0f);
	Style.Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
	Style.Colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.14f, 0.98f);
	Style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
	Style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
	Style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
	Style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
	Style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
	Style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
	Style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
	Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
	Style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.32f, 0.35f, 1.0f);
	Style.Colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
	Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.27f, 1.0f);
	Style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.28f, 0.31f, 1.0f);
	Style.Colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
	Style.Colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
}

void FObjViewerPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UObjViewerEngine* InEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
	ApplyEditorColorTheme();
	ApplyEditorTabStyle();

	Window = InWindow;
	Engine = InEngine;

	ImGuiStyle& Style = ImGui::GetStyle();
	Style.WindowPadding.x = (std::max)(Style.WindowPadding.x, 12.0f);
	Style.WindowPadding.y = (std::max)(Style.WindowPadding.y, 10.0f);
	Style.FramePadding.x = (std::max)(Style.FramePadding.x, 8.0f);
	Style.FramePadding.y = (std::max)(Style.FramePadding.y, 6.0f);
	Style.ItemSpacing.x = (std::max)(Style.ItemSpacing.x, 10.0f);
	Style.ItemSpacing.y = (std::max)(Style.ItemSpacing.y, 8.0f);
	Style.CellPadding.x = (std::max)(Style.CellPadding.x, 8.0f);
	Style.CellPadding.y = (std::max)(Style.CellPadding.y, 6.0f);

	const FString FontPath = FResourceManager::Get().ResolvePath(FName("Default.Font.UI"));
	const std::filesystem::path UIFontPath = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(FontPath);
	const FString UIFontPathAbsolute = FPaths::ToUtf8(UIFontPath.lexically_normal().wstring());
	IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
	TitleBarFont = IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	if (std::filesystem::exists("C:/Windows/Fonts/segmdl2.ttf"))
	{
		ImFontConfig IconFontConfig{};
		IconFontConfig.PixelSnapH = true;
		WindowControlIconFont = IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segmdl2.ttf", 13.0f, &IconFontConfig);
	}

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());
}

void FObjViewerPanel::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FObjViewerPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const ImVec2 ViewportMin = MainViewport->Pos;
	const ImVec2 ViewportMax(MainViewport->Pos.x + MainViewport->Size.x, MainViewport->Pos.y + MainViewport->Size.y);
	const ImVec2 FrameMin(MainViewport->Pos.x + ObjViewerOuterPadding, MainViewport->Pos.y + ObjViewerOuterPadding);
	const ImVec2 FrameMax(MainViewport->Pos.x + MainViewport->Size.x - ObjViewerOuterPadding, MainViewport->Pos.y + MainViewport->Size.y - ObjViewerOuterPadding);
	ImDrawList* BackgroundDrawList = ImGui::GetBackgroundDrawList(const_cast<ImGuiViewport*>(MainViewport));
	BackgroundDrawList->AddRectFilled(ViewportMin, ViewportMax, IM_COL32(5, 5, 5, 255));
	BackgroundDrawList->AddRectFilled(FrameMin, FrameMax, IM_COL32(5, 5, 5, 255), ObjViewerCornerRadius);

	RenderTitleBar();
	RenderDockSpace();

	RenderMeshList();
	RenderImportPopup();
	RenderPreviewViewport(DeltaTime);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FObjViewerPanel::RenderTitleBar()
{
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float RightControlsWidth = ObjViewerWindowControlButtonWidth * 3.0f + ObjViewerWindowControlSpacing * 2.0f;

	ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + ObjViewerOuterPadding, MainViewport->Pos.y + ObjViewerOuterPadding), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(MainViewport->Size.x - ObjViewerOuterPadding * 2.0f, ObjViewerTitleBarHeight), ImGuiCond_Always);
	ImGui::SetNextWindowViewport(MainViewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));

	const ImGuiWindowFlags TitleBarFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	if (!ImGui::Begin("##ObjViewerCustomTitleBar", nullptr, TitleBarFlags))
	{
		ImGui::End();
		ImGui::PopStyleVar(4);
		return;
	}

	if (TitleBarFont)
	{
		ImGui::PushFont(TitleBarFont);
	}

	const float WindowWidth = ImGui::GetWindowWidth();
	const float ContentStartY = (std::max)(0.0f, floorf((ObjViewerTitleBarHeight - ImGui::GetFrameHeight()) * 0.5f));

	// 좌측 타이틀 텍스트
	ImGui::SetCursorPos(ImVec2(14.0f, ContentStartY));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.87f, 1.0f));
	ImGui::TextUnformatted("Lunatic Obj Viewer");
	ImGui::PopStyleColor();

	// 우측 윈도우 컨트롤 (Min / Max / Close)
	const float RightControlsStartX = WindowWidth - RightControlsWidth;
	const float ControlsContentY = (std::max)(0.0f, floorf((ObjViewerTitleBarHeight - ObjViewerWindowControlHeight) * 0.5f));

	if (Window)
	{
		ImGui::SetCursorPos(ImVec2(RightControlsStartX, ControlsContentY));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.24f, 0.26f, 1.0f));

		if (WindowControlIconFont)
		{
			ImGui::PushFont(WindowControlIconFont);
		}
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.58f));
		if (ImGui::Button(GetWindowControlIconMinimize(), ImVec2(ObjViewerWindowControlButtonWidth, ObjViewerWindowControlHeight)))
		{
			Window->Minimize();
		}
		ImGui::PopStyleVar();
		ImGui::SameLine(0.0f, ObjViewerWindowControlSpacing);
		if (ImGui::Button(Window->IsWindowMaximized() ? GetWindowControlIconRestore() : GetWindowControlIconMaximize(), ImVec2(ObjViewerWindowControlButtonWidth, ObjViewerWindowControlHeight)))
		{
			Window->ToggleMaximize();
		}
		ImGui::SameLine(0.0f, ObjViewerWindowControlSpacing);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.16f, 0.16f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.58f, 0.10f, 0.10f, 1.0f));
		if (ImGui::Button(GetWindowControlIconClose(), ImVec2(ObjViewerWindowControlButtonWidth, ObjViewerWindowControlHeight)))
		{
			Window->Close();
		}
		if (WindowControlIconFont)
		{
			ImGui::PopFont();
		}
		ImGui::PopStyleColor(2);
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
	}

	if (TitleBarFont)
	{
		ImGui::PopFont();
	}

	// 드래그/컨트롤 영역 등록 — WM_NCHITTEST 가 사용
	const float DragRegionStartX = 14.0f + ImGui::CalcTextSize("Lunatic Obj Viewer").x + 16.0f;
	const float DragRegionEndX = WindowWidth - RightControlsWidth - 16.0f;
	const float DragRegionWidth = DragRegionEndX - DragRegionStartX;

	const float TitleBarClientOriginX = ObjViewerOuterPadding;
	const float TitleBarClientOriginY = ObjViewerOuterPadding;
	const float TitleBarControlRegionX = TitleBarClientOriginX + (WindowWidth - RightControlsWidth);
	const float TitleBarControlRegionY = TitleBarClientOriginY + ControlsContentY;

	if (Window && DragRegionWidth > 24.0f)
	{
		Window->SetTitleBarDragRegion(
			TitleBarClientOriginX + DragRegionStartX,
			TitleBarClientOriginY,
			DragRegionWidth,
			ObjViewerTitleBarHeight);
		Window->SetTitleBarControlRegion(
			TitleBarControlRegionX,
			TitleBarControlRegionY,
			RightControlsWidth,
			ObjViewerWindowControlHeight);
	}
	else if (Window)
	{
		Window->ClearTitleBarDragRegion();
		Window->ClearTitleBarControlRegion();
	}

	ImGui::End();
	ImGui::PopStyleVar(4);
}

void FObjViewerPanel::RenderDockSpace()
{
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGuiWindowClass DockspaceWindowClass{};
	DockspaceWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;

	const float ContentTop = ObjViewerOuterPadding + ObjViewerTitleBarHeight;
	ImGui::SetNextWindowPos(
		ImVec2(MainViewport->Pos.x + ObjViewerOuterPadding,
			MainViewport->Pos.y + ContentTop),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(
		ImVec2(MainViewport->Size.x - ObjViewerOuterPadding * 2.0f,
			MainViewport->Size.y - ContentTop - ObjViewerOuterPadding),
		ImGuiCond_Always);
	ImGui::SetNextWindowViewport(MainViewport->ID);

	const ImGuiWindowFlags DockspaceWindowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin("##ObjViewerDockSpaceHost", nullptr, DockspaceWindowFlags))
	{
		ImGui::DockSpace(ImGui::GetID("##ObjViewerDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &DockspaceWindowClass);
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
}

void FObjViewerPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	// 프리뷰 뷰포트 위에서는 ImGui 마우스 캡처 해제
	bool bWantMouse = IO.WantCaptureMouse;
	FObjViewerViewportClient* VC = Engine ? Engine->GetViewportClient() : nullptr;
	if (VC)
	{
		// ImGui hover 체크를 통해 뷰포트 영역 감지
		// (뷰포트 ImGui::Image 위에 InvisibleButton이 있으므로 그걸로 판단)
		bWantMouse = IO.WantCaptureMouse;
	}
}

void FObjViewerPanel::RenderMeshList()
{
	ImGui::Begin("Mesh List");

	// OBJ Files 섹션
	if (ImGui::CollapsingHeader("OBJ Files", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();

		for (int32 i = 0; i < static_cast<int32>(ObjFiles.size()); ++i)
		{
			bool bSelected = (i == SelectedObjIndex);
			if (ImGui::Selectable(ObjFiles[i].DisplayName.c_str(), bSelected))
			{
				if (SelectedObjIndex != i)
				{
					SelectedObjIndex = i;
					if (Engine)
					{
						Engine->LoadPreviewMesh(ObjFiles[i].FullPath);
					}
				}
			}
		}

		// Import 버튼 — 선택된 OBJ가 있을 때만 활성
		bool bHasSelection = SelectedObjIndex >= 0 && SelectedObjIndex < static_cast<int32>(ObjFiles.size());
		if (!bHasSelection) ImGui::BeginDisabled();
		if (ImGui::Button("Import..."))
		{
			PendingImportOptions = FImportOptions::Default();
			bShowImportPopup = true;
			ImGui::OpenPopup("Import Options");
		}
		if (!bHasSelection) ImGui::EndDisabled();
	}

	ImGui::Separator();

	if (ImGui::CollapsingHeader("FBX Summary", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::InputText("FBX Path", FbxPathInput, IM_ARRAYSIZE(FbxPathInput));
		ImGui::SameLine();

		const bool bHasFbxPath = FbxPathInput[0] != '\0';
		if (!bHasFbxPath) ImGui::BeginDisabled();
		if (ImGui::Button("Log Summary"))
		{
			if (Engine)
			{
				Engine->LogFbxSceneSummary(FbxPathInput);
			}
		}
		if (ImGui::Button("Preview FBX Static Mesh"))
		{
			if (Engine)
			{
				Engine->LoadPreviewFbxStaticMesh(FbxPathInput);
			}
		}

		if (!bHasFbxPath) ImGui::EndDisabled();
	}

	ImGui::Separator();

	// Cached Meshes (.bin) 섹션
	if (ImGui::CollapsingHeader("Cached Meshes (.bin)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<FMeshAssetListItem>& MeshFiles = FObjManager::GetAvailableMeshFiles();

		for (int32 i = 0; i < static_cast<int32>(MeshFiles.size()); ++i)
		{
			bool bSelected = (i == SelectedMeshIndex);
			if (ImGui::Selectable(MeshFiles[i].DisplayName.c_str(), bSelected))
			{
				if (SelectedMeshIndex != i)
				{
					SelectedMeshIndex = i;
					if (Engine)
					{
						Engine->LoadPreviewMesh(MeshFiles[i].FullPath);
					}
				}
			}
		}
	}

	ImGui::End();
}

void FObjViewerPanel::RenderImportPopup()
{
	if (!bShowImportPopup) return;

	ImGui::OpenPopup("Import Options");

	ImVec2 Center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(Center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Import Options", &bShowImportPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Scale
		ImGui::InputFloat("Scale", &PendingImportOptions.Scale, 0.01f, 1.0f, "%.4f");
		ImGui::SameLine();
		if (ImGui::SmallButton("cm->m"))
		{
			PendingImportOptions.Scale = 0.01f;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("1:1"))
		{
			PendingImportOptions.Scale = 1.0f;
		}

		// Forward Axis
		const char* AxisLabels[] = { "X", "-X", "Y", "-Y", "Z", "-Z" };
		int AxisIndex = static_cast<int>(PendingImportOptions.ForwardAxis);
		if (ImGui::Combo("Forward Axis", &AxisIndex, AxisLabels, IM_ARRAYSIZE(AxisLabels)))
		{
			PendingImportOptions.ForwardAxis = static_cast<EForwardAxis>(AxisIndex);
		}

		// Winding Order
		const char* WindingLabels[] = { "CCW -> CW (DirectX)", "Keep Original" };
		int WindingIndex = static_cast<int>(PendingImportOptions.WindingOrder);
		if (ImGui::Combo("Winding Order", &WindingIndex, WindingLabels, IM_ARRAYSIZE(WindingLabels)))
		{
			PendingImportOptions.WindingOrder = static_cast<EWindingOrder>(WindingIndex);
		}

		ImGui::Separator();

		// Import / Cancel 버튼
		if (ImGui::Button("Import", ImVec2(120, 0)))
		{
			const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();
			if (Engine && SelectedObjIndex >= 0 && SelectedObjIndex < static_cast<int32>(ObjFiles.size()))
			{
				Engine->ImportObjWithOptions(ObjFiles[SelectedObjIndex].FullPath, PendingImportOptions);
			}
			bShowImportPopup = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			bShowImportPopup = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void FObjViewerPanel::RenderPreviewViewport(float DeltaTime)
{
	ImGui::Begin("Preview");

	FObjViewerViewportClient* VC = Engine ? Engine->GetViewportClient() : nullptr;

	if (VC)
	{
		ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImVec2 Size = ImGui::GetContentRegionAvail();

		if (Size.x > 0 && Size.y > 0)
		{
			VC->SetViewportRect(Pos.x, Pos.y, Size.x, Size.y);
			VC->RenderViewportImage();

			// 투명 버튼으로 ImGui 마우스 캡처를 뷰포트 위에서 해제
			ImGui::InvisibleButton("##PreviewViewport", Size);
			if (ImGui::IsItemHovered())
			{
				// InputSystem::Get().GetGuiInputState().bUsingMouse = false;
			}
		}
	}

	ImGui::End();
}
