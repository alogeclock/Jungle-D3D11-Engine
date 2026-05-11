#include "PreviewViewportWidget.h"

#include "Editor/Viewport/Preview/PreviewViewportClient.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

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
	ViewportClient->SetViewportRect(Min.x, Min.y, Max.x - Min.x, Max.y - Min.y);
	ViewportClient->SetHovered(ImGui::IsItemHovered());
	ViewportClient->SetActive(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
}
