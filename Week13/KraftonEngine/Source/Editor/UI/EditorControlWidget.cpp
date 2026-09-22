#include "Editor/UI/EditorControlWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "ImGui/imgui.h"
#include "Math/MathUtils.h"

void FEditorControlWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	if (!ImGui::Begin("Control Panel"))
	{
		ImGui::End();
		return;
	}

	// 액터 배치 기능은 뷰포트 Place Actor 메뉴로 이동하고, Control Panel은 카메라 제어만 유지한다.
	// D.3: ViewTransform 이 SoT — Camera 컴포넌트 setter 직접 호출 안 함.
	FLevelEditorViewportClient* VC = EditorEngine->GetActiveViewport();
	if (!VC)
	{
		ImGui::End();
		return;
	}

	FViewportCameraTransform& VT = VC->GetViewTransform();
	bool bChanged = false;

	float CameraFOV_Deg = VT.FOV * RAD_TO_DEG;
	if (ImGui::DragFloat("Camera FOV", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
	{
		VT.FOV = CameraFOV_Deg * DEG_TO_RAD;
		bChanged = true;
	}

	float OrthoWidth = VT.OrthoZoom;
	if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
	{
		VT.OrthoZoom = Clamp(OrthoWidth, 0.1f, 1000.0f);
		bChanged = true;
	}

	float CameraLocation[3] = { VT.ViewLocation.X, VT.ViewLocation.Y, VT.ViewLocation.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
	{
		VT.ViewLocation = FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]);
		bChanged = true;
	}

	float CameraRotation[3] = { VT.ViewRotation.Roll, VT.ViewRotation.Pitch, VT.ViewRotation.Yaw };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		VT.ViewRotation = FRotator(CameraRotation[1], CameraRotation[2], CameraRotation[0]);
		bChanged = true;
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Depth Of Field");

	bool bRenderDOF = VC->GetRenderOptions().ShowFlags.bDepthOfField;
	if (ImGui::Checkbox("Render DOF", &bRenderDOF))
	{
		VC->GetRenderOptions().ShowFlags.bDepthOfField = bRenderDOF;
	}

	FCameraViewSettings& CameraViewSettings = VT.CameraViewSettings;
	FCameraDOFSettings& DOFSettings = CameraViewSettings.DOFSettings;
	if (ImGui::Checkbox("Viewport DOF", &DOFSettings.bEnableDOF))
	{
		if (DOFSettings.bEnableDOF)
		{
			CameraViewSettings.FocusSettings.FocusMode = ECameraFocusMode::CFM_Manual;
			VC->GetRenderOptions().ShowFlags.bDepthOfField = true;
		}
		bChanged = true;
	}

	float FocusDistance = CameraViewSettings.FocusSettings.ManualFocusDistance;
	if (ImGui::DragFloat("Focus Distance", &FocusDistance, 0.1f, 0.01f, 100000.0f))
	{
		CameraViewSettings.FocusSettings.FocusMode = ECameraFocusMode::CFM_Manual;
		CameraViewSettings.FocusSettings.ManualFocusDistance = Clamp(FocusDistance, 0.01f, 100000.0f);
		bChanged = true;
	}

	float FocalLength = DOFSettings.FocalLength;
	if (ImGui::DragFloat("Focal Length", &FocalLength, 0.1f, 0.1f, 300.0f))
	{
		DOFSettings.FocalLength = Clamp(FocalLength, 0.1f, 300.0f);
		bChanged = true;
	}

	float FocusRange = DOFSettings.FocusRange;
	if (ImGui::DragFloat("Focus Range", &FocusRange, 0.1f, 0.0f, 100000.0f))
	{
		DOFSettings.FocusRange = Clamp(FocusRange, 0.0f, 100000.0f);
		bChanged = true;
	}

	float Aperture = DOFSettings.Aperture;
	if (ImGui::DragFloat("Aperture F-Stop", &Aperture, 0.1f, 0.1f, 64.0f))
	{
		DOFSettings.Aperture = Clamp(Aperture, 0.1f, 64.0f);
		bChanged = true;
	}

	float MaxBlurRadius = DOFSettings.MaxBlurRadius;
	if (ImGui::DragFloat("Max Blur Radius", &MaxBlurRadius, 0.1f, 0.0f, 12.0f))
	{
		DOFSettings.MaxBlurRadius = Clamp(MaxBlurRadius, 0.0f, 12.0f);
		bChanged = true;
	}

	float NearCoCScale = DOFSettings.NearCoCScale;
	if (ImGui::DragFloat("Near CoC Scale", &NearCoCScale, 0.01f, 0.0f, 1.0f))
	{
		DOFSettings.NearCoCScale = Clamp(NearCoCScale, 0.0f, 1.0f);
		bChanged = true;
	}

	if (bChanged)
	{
		VC->NotifyViewTransformChanged();
	}

	ImGui::End();
}
