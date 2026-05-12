#include "Editor/UI/EditorCommonWidgetUtils.h"

#include "ImGui/imgui.h"

#include <algorithm>

namespace
{
	constexpr ImVec4 DetailsVectorLabelColor = ImVec4(0.83f, 0.84f, 0.87f, 1.0f);
	constexpr ImVec4 DetailsVectorFieldBg = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.0f);
	constexpr ImVec4 DetailsVectorFieldHoverBg = ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 1.0f);
	constexpr ImVec4 DetailsVectorFieldActiveBg = ImVec4(20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f);
	constexpr ImVec4 DetailsVectorFieldBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
	constexpr ImVec4 DetailsVectorResetButtonColor = ImVec4(0.22f, 0.22f, 0.23f, 1.0f);
	constexpr ImVec4 DetailsVectorResetButtonHoveredColor = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
	constexpr ImVec4 DetailsVectorResetButtonActiveColor = ImVec4(0.36f, 0.36f, 0.38f, 1.0f);
	constexpr ImVec4 DetailsVectorResetButtonBorderColor = ImVec4(0.52f, 0.52f, 0.55f, 0.95f);
	constexpr float DetailsVectorLabelWidth = 124.0f;
	constexpr float DetailsPropertyLabelWidth = 124.0f;
	constexpr float DetailsVectorResetSpacing = 6.0f;

	void PushVectorFieldStyle()
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, DetailsVectorFieldBg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, DetailsVectorFieldHoverBg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, DetailsVectorFieldActiveBg);
		ImGui::PushStyleColor(ImGuiCol_Border, DetailsVectorFieldBorder);
	}

	void PopVectorFieldStyle()
	{
		ImGui::PopStyleColor(4);
	}

	void PushVectorResetButtonStyle()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, DetailsVectorResetButtonColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DetailsVectorResetButtonHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, DetailsVectorResetButtonActiveColor);
		ImGui::PushStyleColor(ImGuiCol_Border, DetailsVectorResetButtonBorderColor);
	}

	void PopVectorResetButtonStyle()
	{
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar();
	}

	float GetAxisFieldWidth(int32 AxisCount, float AdditionalReservedWidth = 0.0f)
	{
		const float AvailableWidth = ImGui::GetContentRegionAvail().x - AdditionalReservedWidth;
		const float InterAxisSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float BarWidth = 3.0f;
		const float BarSpacing = 3.0f;
		const float TotalReserved = (BarWidth + BarSpacing) * static_cast<float>(AxisCount);
		const float TotalSpacing = InterAxisSpacing * static_cast<float>((std::max)(0, AxisCount - 1));
		return (std::max)(22.0f, (AvailableWidth - TotalReserved - TotalSpacing) / static_cast<float>(AxisCount));
	}

	bool DrawColoredFloatAxes(const char* Label, float* Values, int32 AxisCount, float Speed, bool bShowReset, const float* ResetValues)
	{
	    static constexpr const char* FieldIds[4] = { "##X", "##Y", "##Z", "##W" };
	    static constexpr ImVec4 AxisColors[4] =
	    {
	       ImVec4(0.85f, 0.22f, 0.22f, 1.0f),
	       ImVec4(0.36f, 0.74f, 0.25f, 1.0f),
	       ImVec4(0.23f, 0.54f, 0.92f, 1.0f),
	       ImVec4(0.72f, 0.72f, 0.72f, 1.0f)
	    };

	    ImGui::PushID(Label);
	    ImGui::AlignTextToFramePadding();
	    ImGui::PushStyleColor(ImGuiCol_Text, DetailsVectorLabelColor);
	    ImGui::TextUnformatted(Label);
	    ImGui::PopStyleColor();
	    ImGui::SameLine(DetailsVectorLabelWidth);

	    const float ResetButtonWidth = bShowReset ? ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f : 0.0f;
	    
	    const float TotalAvailWidth = ImGui::GetContentRegionAvail().x - (bShowReset ? (ResetButtonWidth + DetailsVectorResetSpacing) : 0.0f);
	    const float Spacing = ImGui::GetStyle().ItemSpacing.x;
	    const float FieldWidth = (TotalAvailWidth - (Spacing * (AxisCount - 1))) / (float)AxisCount;

	    bool bChanged = false;
	    for (int32 Axis = 0; Axis < AxisCount; ++Axis)
	    {
	       if (Axis > 0) ImGui::SameLine();

	       const ImVec2 Start = ImGui::GetCursorScreenPos();
	       const float BarWidth = 3.0f;
	       const float BarGap = 3.0f;
	       
	       ImGui::GetWindowDrawList()->AddRectFilled(Start, ImVec2(Start.x + BarWidth, Start.y + ImGui::GetFrameHeight()),
	          ImGui::ColorConvertFloat4ToU32(AxisColors[Axis]), 2.0f);
	       ImGui::SetCursorScreenPos(ImVec2(Start.x + BarWidth + BarGap, Start.y));

	       PushVectorFieldStyle();
	       ImGui::SetNextItemWidth((std::max)(16.0f, FieldWidth - (BarWidth + BarGap)));
	       bChanged |= ImGui::DragFloat(FieldIds[Axis], &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
	       PopVectorFieldStyle();
	    }

	    if (bShowReset)
	    {
	       ImGui::SameLine(0.0f, DetailsVectorResetSpacing);
	       PushVectorResetButtonStyle();
	       if (ImGui::Button("RESET"))
	       {
	          for (int32 Axis = 0; Axis < AxisCount; ++Axis)
	             Values[Axis] = ResetValues ? ResetValues[Axis] : 0.0f;
	          bChanged = true;
	       }
	       PopVectorResetButtonStyle();
	    }

	    ImGui::PopID();
	    return bChanged;
	}
}

namespace FEditorCommonWidgetUtils
{
	bool DrawLabeledField(const char* Label, const std::function<bool()>& DrawField)
	{
		const float RowStartX = ImGui::GetCursorPosX();
		const float TotalWidth = ImGui::GetContentRegionAvail().x;
		const float LabelTextWidth = ImGui::CalcTextSize(Label).x;
		const ImGuiStyle& Style = ImGui::GetStyle();
		const float DesiredLabelWidth = (std::max)(DetailsPropertyLabelWidth, LabelTextWidth + Style.ItemSpacing.x + Style.FramePadding.x * 2.0f);
		const float MaxLabelWidth = (std::max)(DetailsPropertyLabelWidth, TotalWidth * 0.48f);
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

	void DrawReadOnlyField(const char* Label, const FString& Value)
	{
		DrawLabeledField(Label, [&]()
		{
			ImGui::TextDisabled("%s", Value.c_str());
			return false;
		});
	}

	bool DrawColoredFloat2(const char* Label, float Values[3], float Speed, bool bShowReset, const float* ResetValues)
	{
		return DrawColoredFloatAxes(Label, Values, 2, Speed, bShowReset, ResetValues);
	}

	bool DrawColoredFloat3(const char* Label, float Values[3], float Speed, bool bShowReset, const float* ResetValues)
	{
		return DrawColoredFloatAxes(Label, Values, 3, Speed, bShowReset, ResetValues);
	}

	bool DrawColoredFloat4(const char* Label, float Values[4], float Speed, bool bShowReset, const float* ResetValues)
	{
		return DrawColoredFloatAxes(Label, Values, 4, Speed, bShowReset, ResetValues);
	}

	bool DrawNamedFloat4(const char* Label, float Values[4], float Speed, const char* AxisLabels[4], bool bShowReset, const float* ResetValues)
	{
	    static constexpr const char* FieldIds[4] = { "##L", "##T", "##R", "##B" };

	    ImGui::PushID(Label);
	    ImGui::AlignTextToFramePadding();
	    ImGui::PushStyleColor(ImGuiCol_Text, DetailsVectorLabelColor);
	    ImGui::TextUnformatted(Label);
	    ImGui::PopStyleColor();
	    ImGui::SameLine(DetailsVectorLabelWidth);

	    const float ResetButtonWidth = bShowReset ? ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f : 0.0f;
	    const float TotalAvailWidth = ImGui::GetContentRegionAvail().x - (bShowReset ? (ResetButtonWidth + DetailsVectorResetSpacing) : 0.0f);
	    const float Spacing = ImGui::GetStyle().ItemSpacing.x;
	    const float SectionWidth = (TotalAvailWidth - (Spacing * 3)) / 4.0f;

	    bool bChanged = false;
	    for (int32 Axis = 0; Axis < 4; ++Axis)
	    {
	       if (Axis > 0) ImGui::SameLine();

	       const char* AxisLabel = (AxisLabels && AxisLabels[Axis]) ? AxisLabels[Axis] : "";
	       const float AxisLabelWidth = AxisLabel[0] != '\0' ? ImGui::CalcTextSize(AxisLabel).x + 4.0f : 0.0f;
	       
	       if (AxisLabel[0] != '\0')
	       {
	          ImGui::AlignTextToFramePadding();
	          ImGui::TextUnformatted(AxisLabel);
	          ImGui::SameLine(0.0f, 4.0f);
	       }

	       PushVectorFieldStyle();
	       ImGui::SetNextItemWidth((std::max)(18.0f, SectionWidth - AxisLabelWidth));
	       bChanged |= ImGui::DragFloat(FieldIds[Axis], &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
	       PopVectorFieldStyle();
	    }

	    if (bShowReset)
	    {
	       ImGui::SameLine(0.0f, DetailsVectorResetSpacing);
	       PushVectorResetButtonStyle();
	       if (ImGui::Button("RESET"))
	       {
	          for (int32 Axis = 0; Axis < 4; ++Axis)
	             Values[Axis] = ResetValues ? ResetValues[Axis] : 0.0f;
	          bChanged = true;
	       }
	       PopVectorResetButtonStyle();
	    }

	    ImGui::PopID();
	    return bChanged;
	}
}
