#pragma once

#include "Core/CoreTypes.h"

#include <functional>

namespace FEditorCommonWidgetUtils
{
	bool DrawLabeledField(const char* Label, const std::function<bool()>& DrawField);
	void DrawReadOnlyField(const char* Label, const FString& Value);

	bool DrawColoredFloat2(const char* Label, float Values[3], float Speed, bool bShowReset = true, const float* ResetValues = nullptr);
	bool DrawColoredFloat3(const char* Label, float Values[3], float Speed, bool bShowReset = true, const float* ResetValues = nullptr);
	bool DrawColoredFloat4(const char* Label, float Values[4], float Speed, bool bShowReset = true, const float* ResetValues = nullptr);
	bool DrawNamedFloat4(const char* Label, float Values[4], float Speed, const char* AxisLabels[4], bool bShowReset = true, const float* ResetValues = nullptr);
}
