#pragma once

class FPreviewViewportClient;

class FPreviewViewportWidget
{
public:
	void SetViewportClient(FPreviewViewportClient* InClient);
	void Render(float DeltaTime);

private:
	FPreviewViewportClient* ViewportClient = nullptr;
};
