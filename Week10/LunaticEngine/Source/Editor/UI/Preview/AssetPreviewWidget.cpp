#include "AssetPreviewWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorCommonWidgetUtils.h"
#include "Editor/Viewport/Preview/PreviewViewportClient.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

FAssetPreviewWidget::~FAssetPreviewWidget() = default;

// 입력 정보를 초기화합니다.
void FAssetPreviewWidget::ClearInputCapture()
{
	if (PreviewViewportClient)
	{
		PreviewViewportClient->SetHovered(false);
		PreviewViewportClient->SetActive(false);
	}
	bCapturingInput = false;
}

// Preview ViewportClient를 프리뷰 위젯에 바인딩합니다.
std::unique_ptr<FPreviewViewportClient> FAssetPreviewWidget::CreatePreviewViewportClient()
{
	return std::make_unique<FPreviewViewportClient>();
}

FPreviewViewportClient* FAssetPreviewWidget::EnsurePreviewViewportClient()
{
	if (!PreviewViewportClient)
	{
		PreviewViewportClient = CreatePreviewViewportClient();
		ViewportWidget.SetViewportClient(PreviewViewportClient.get());
	}

	return PreviewViewportClient.get();
}

void FAssetPreviewWidget::ReleasePreviewViewportClient()
{
	ViewportWidget.SetViewportClient(nullptr);
	PreviewViewportClient.reset();
}

// Preview ViewportClient를 엔진에 등록합니다.
void FAssetPreviewWidget::RegisterPreviewClient()
{
	FPreviewViewportClient* Client = GetPreviewViewportClient();
	if (!Engine || bRegistered || !Client)
	{
		return;
	}

	Engine->RegisterPreviewViewportClient(Client);
	bRegistered = true;
}

// Preview ViewportClient를 엔진에서 등록 해제합니다.
void FAssetPreviewWidget::UnregisterPreviewClient()
{
	FPreviewViewportClient* Client = GetPreviewViewportClient();
	if (!Engine || !bRegistered || !Client)
	{
		return;
	}

	Engine->UnregisterPreviewViewportClient(Client);
	bRegistered = false;
}

// Preview ViewportClient가 이미 켜져 있는지 확인합니다. (켜져 있다면 이미 켜진 창에 탭을 추가합니다.)
bool FAssetPreviewWidget::IsMultiViewportEnabled() const
{
	return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

// 멀티 뷰포트 모드일 때, 새 창이 열리는 위치를 조금씩 수정하여 겹치지 않도록 합니다.
void FAssetPreviewWidget::SetNextPreviewEditorWindowPolicy() const
{
	if (!IsMultiViewportEnabled())
	{
		return;
	}

	ImGuiWindowClass WindowClass{};
	WindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
	WindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
	ImGui::SetNextWindowClass(&WindowClass);

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float Offset = static_cast<float>((std::max)(0, EditorInstanceId - 1) % 6) * 28.0f;
	ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + 96.0f + Offset, MainViewport->Pos.y + 80.0f + Offset), ImGuiCond_FirstUseEver);
}

// ────────────────────────────────────────────────────────────
// ImGui 기반 UI Rendering
// ────────────────────────────────────────────────────────────

// Bone, Transform 패널과 같이 접고 펼 수 있는 헤더 섹션을 생성합니다.
bool FAssetPreviewWidget::BeginPreviewDetailsSection(const char* SectionName) const
{
	const std::string HeaderId = std::string(SectionName) + "##PreviewDetailsSection";
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.76f, 0.78f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
	const bool bSectionOpen = ImGui::CollapsingHeader(HeaderId.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);
	return bSectionOpen;
}

// 이름, 입력 칸이 오는 표준 레이아웃 UI
bool FAssetPreviewWidget::DrawPreviewLabeledField(const char* Label, const std::function<bool()>& DrawField) const
{
	return FEditorCommonWidgetUtils::DrawLabeledField(Label, DrawField);
}

// Preview Widget 읽기 전용 필드
void FAssetPreviewWidget::DrawPreviewReadOnlyField(const char* Label, const FString& Value) const
{
	FEditorCommonWidgetUtils::DrawReadOnlyField(Label, Value);
}

// FTransform 데이터를 X, Y, Z축으로 나눠 나타내는 3개 입력 필드
bool FAssetPreviewWidget::DrawPreviewColoredFloat3(const char* Label, float Values[3], float Speed, const float* ResetValues) const
{
	return FEditorCommonWidgetUtils::DrawColoredFloat3(Label, Values, Speed, ResetValues != nullptr, ResetValues);
}

// ────────────────────────────────────────────────────────────
// Text Formatting & UI Constants
// ────────────────────────────────────────────────────────────

// Location, Rotation, Scale을 나란히 표시하는 UI
void FAssetPreviewWidget::DrawTransformMatrixRows(const char* SectionName, const FMatrix& Matrix) const
{
	if (!BeginPreviewDetailsSection(SectionName))
	{
		return;
	}

	DrawPreviewReadOnlyField("Location", FormatPreviewVector(Matrix.GetLocation()));
	DrawPreviewReadOnlyField("Rotation", FormatPreviewVector(Matrix.GetEuler()));
	DrawPreviewReadOnlyField("Scale", FormatPreviewVector(Matrix.GetScale()));
}

// 파일 경로에서 파일명을 추출해 에디터 창의 제목을 결정하는 유틸리티 함수
FString FAssetPreviewWidget::FormatPreviewVector(const FVector& Value) const
{
	char Buffer[128] = {};
	snprintf(Buffer, sizeof(Buffer), "%.3f, %.3f, %.3f", Value.X, Value.Y, Value.Z);
	return Buffer;
}
