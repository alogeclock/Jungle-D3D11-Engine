#pragma once

#include "Core/CoreTypes.h"

class FPreviewViewportClient;

class FPreviewViewportWidget
{
public:
	void SetViewportClient(FPreviewViewportClient* InClient);
	void Render(float DeltaTime);

private:
	FPreviewViewportClient* ViewportClient = nullptr;
	int32 PreviewGizmoMode = 0;
};
