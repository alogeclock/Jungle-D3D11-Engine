#include "SkeletalMeshEditorWidget.h"

#include "Core/Notification.h"
#include "Editor/EditorEngine.h"
#include "Mesh/SkeletalMesh.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <filesystem>

void FSkeletalMeshEditorWidget::Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice, FWindowsWindow* InWindow)
{
	if (bInitialized)
	{
		return;
	}

	EditorEngine = InEditorEngine;

	Viewport = new FViewport();
	if (!Viewport->Initialize(InDevice, 512, 512))
	{
		delete Viewport;
		Viewport = nullptr;
		FNotificationManager::Get().AddNotification("Failed to initialize SkeletalMesh preview viewport.", ENotificationType::Error, 5.0f);
		return;
	}

	Viewport->SetClient(&ViewportClient);
	ViewportClient.SetViewport(Viewport);
	ViewportClient.Initialize(InWindow);
	ViewportWidget.SetViewportClient(&ViewportClient);
	bInitialized = true;
}

void FSkeletalMeshEditorWidget::Shutdown()
{
	Close();

	ViewportClient.SetViewport(nullptr);
	ViewportWidget.SetViewportClient(nullptr);

	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	EditorEngine = nullptr;
	bInitialized = false;
	bCapturingInput = false;
}

void FSkeletalMeshEditorWidget::OpenSkeletalMesh(USkeletalMesh* InMesh)
{
	if (!bInitialized || !InMesh)
	{
		return;
	}

	EditingMesh = InMesh;
	ViewportClient.SetSkeletalMesh(EditingMesh);
	bOpen = true;
	bCapturingInput = false;
	RegisterPreviewClient();
}

void FSkeletalMeshEditorWidget::Render(float DeltaTime)
{
	if (!bOpen)
	{
		bCapturingInput = false;
		return;
	}

	bool bWindowOpen = bOpen;
	ImGui::SetNextWindowSize(ImVec2(720.0f, 560.0f), ImGuiCond_FirstUseEver);
	const FString WindowTitle = "Skeletal Mesh Editor";
	const bool bVisible = ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, ImGuiWindowFlags_NoCollapse);
	if (!bWindowOpen)
	{
		ImGui::End();
		Close();
		return;
	}

	if (!bVisible)
	{
		ImGui::End();
		ViewportClient.SetHovered(false);
		ViewportClient.SetActive(false);
		bCapturingInput = false;
		return;
	}

	ViewportWidget.Render(DeltaTime);
	bCapturingInput = ViewportClient.IsHovered() || ViewportClient.IsActive();

	ImGui::End();
}

void FSkeletalMeshEditorWidget::ClearInputCapture()
{
	ViewportClient.SetHovered(false);
	ViewportClient.SetActive(false);
	bCapturingInput = false;
}

void FSkeletalMeshEditorWidget::Close()
{
	if (!bOpen && !bRegistered && !EditingMesh)
	{
		return;
	}

	UnregisterPreviewClient();
	ClearInputCapture();
	ViewportClient.SetSkeletalMesh(nullptr);
	EditingMesh = nullptr;
	bOpen = false;
	bCapturingInput = false;
}

void FSkeletalMeshEditorWidget::RegisterPreviewClient()
{
	if (!EditorEngine || bRegistered)
	{
		return;
	}

	EditorEngine->RegisterPreviewViewportClient(&ViewportClient);
	bRegistered = true;
}

void FSkeletalMeshEditorWidget::UnregisterPreviewClient()
{
	if (!EditorEngine || !bRegistered)
	{
		return;
	}

	EditorEngine->UnregisterPreviewViewportClient(&ViewportClient);
	bRegistered = false;
}