#include "AssetPreviewWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/Preview/PreviewViewportClient.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>

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
void FAssetPreviewWidget::SetPreviewViewportClient(FPreviewViewportClient* InViewportClient)
{
	PreviewViewportClient = InViewportClient;
	ViewportWidget.SetViewportClient(InViewportClient);
}

// Preview ViewportClient를 엔진에 등록합니다.
void FAssetPreviewWidget::RegisterPreviewClient(FPreviewViewportClient* InViewportClient)
{
	if (!Engine || bRegistered || !InViewportClient)
	{
		return;
	}

	Engine->RegisterPreviewViewportClient(InViewportClient);
	bRegistered = true;
}

// Preview ViewportClient를 엔진에서 등록 해제합니다.
void FAssetPreviewWidget::UnregisterPreviewClient(FPreviewViewportClient* InViewportClient)
{
	if (!Engine || !bRegistered || !InViewportClient)
	{
		return;
	}

	Engine->UnregisterPreviewViewportClient(InViewportClient);
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
	const float RowStartX = ImGui::GetCursorPosX();
	const float TotalWidth = ImGui::GetContentRegionAvail().x;
	const float LabelTextWidth = ImGui::CalcTextSize(Label).x;
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float DesiredLabelWidth = (std::max)(PreviewDetailsPropertyLabelWidth, LabelTextWidth + Style.ItemSpacing.x + Style.FramePadding.x * 2.0f);
	const float MaxLabelWidth = (std::max)(PreviewDetailsPropertyLabelWidth, TotalWidth * 0.48f);
	const float LabelColumnWidth = (std::min)(DesiredLabelWidth, MaxLabelWidth);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(Label);
	ImGui::SameLine(RowStartX + LabelColumnWidth);

	const float FieldWidth = TotalWidth - LabelColumnWidth;
	if (FieldWidth > 0.0f)
	{
		ImGui::SetNextItemWidth(FieldWidth);
	}

	return DrawField();
}

// Preview Widget 읽기 전용 필드
void FAssetPreviewWidget::DrawPreviewReadOnlyField(const char* Label, const FString& Value) const
{
	DrawPreviewLabeledField(Label, [&]()
	{
		ImGui::TextDisabled("%s", Value.c_str());
		return false;
	});
}

// FTransform 데이터를 X, Y, Z축으로 나눠 나타내는 3개 입력 필드
bool FAssetPreviewWidget::DrawPreviewColoredFloat3(const char* Label, float Values[3], float Speed, const float* ResetValues) const
{
	return DrawPreviewLabeledField(Label, [&]()
	{
		bool bChanged = false;
		const char* AxisLabels[3] = { "X", "Y", "Z" };
		const ImVec4 AxisColors[3] =
		{
			ImVec4(0.86f, 0.24f, 0.24f, 1.0f),
			ImVec4(0.38f, 0.72f, 0.28f, 1.0f),
			ImVec4(0.28f, 0.48f, 0.92f, 1.0f)
		};
		const float ResetButtonWidth = ResetValues ? 48.0f : 0.0f;
		const float AvailableWidth = ImGui::GetContentRegionAvail().x - ResetButtonWidth;
		const float AxisWidth = (std::max)(36.0f, (AvailableWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f);

		ImGui::PushID(Label);
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			if (AxisIndex > 0)
			{
				ImGui::SameLine();
			}

			ImGui::BeginGroup();
			ImGui::PushStyleColor(ImGuiCol_Text, AxisColors[AxisIndex]);
			ImGui::TextUnformatted(AxisLabels[AxisIndex]);
			ImGui::PopStyleColor();
			ImGui::SameLine(0.0f, 4.0f);
			ImGui::SetNextItemWidth((std::max)(20.0f, AxisWidth - 18.0f));
			
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 33.0f / 255.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f));
			
			char FieldId[8] = {};
			snprintf(FieldId, sizeof(FieldId), "##%s", AxisLabels[AxisIndex]);
			bChanged |= ImGui::DragFloat(FieldId, &Values[AxisIndex], Speed, 0.0f, 0.0f, "%.3f");
			
			ImGui::PopStyleColor(4);
			
			ImGui::EndGroup();
		}

		if (ResetValues)
		{
			ImGui::SameLine();
			if (ImGui::Button("Reset", ImVec2(ResetButtonWidth, 0.0f)))
			{
				Values[0] = ResetValues[0];
				Values[1] = ResetValues[1];
				Values[2] = ResetValues[2];
				bChanged = true;
			}
		}

		ImGui::PopID();
		return bChanged;
	});
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