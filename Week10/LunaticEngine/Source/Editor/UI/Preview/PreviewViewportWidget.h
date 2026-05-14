#pragma once

#include "Core/CoreTypes.h"

class FPreviewViewportClient;
class UEditorEngine;

class FPreviewViewportWidget
{
public:
	void SetViewportClient(FPreviewViewportClient* InClient);
	void Render(float DeltaTime);

protected:
	UEditorEngine* EditorEngine = nullptr; // 현재 참조하고 있는 엔진, 이외의 Widget 구조를 참고

private:
	FPreviewViewportClient* ViewportClient = nullptr;
	int32 PreviewGizmoMode = 0;
	uint32 LastRequestedRenderWidth = 0;
	uint32 LastRequestedRenderHeight = 0;
	double LastResizeRequestTimeSeconds = -1.0;
};
