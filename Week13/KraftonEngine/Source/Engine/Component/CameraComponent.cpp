#include "Component/CameraComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Render/Types/MinimalViewInfo.h"
#include <cmath>

namespace
{
	float ResolveFocusDistance(const FCameraViewSettings& Settings, const FVector& CameraLocation)
	{
		switch (Settings.FocusSettings.FocusMode)
		{
		case ECameraFocusMode::CFM_TrackingActor:
			if (Settings.FocusSettings.TrackingTarget)
			{
				return (Settings.FocusSettings.TrackingTarget->GetActorLocation() - CameraLocation).Length();
			}
			return Settings.FocusSettings.ManualFocusDistance;
		case ECameraFocusMode::CFM_ScreenCenter:
		case ECameraFocusMode::CFM_Manual:
			return Settings.FocusSettings.ManualFocusDistance;
		case ECameraFocusMode::CFM_None:
		default:
			return 0.0f;
		}
	}

	FCameraDepthViewData ResolveCameraDepthViewData(
		const FCameraViewSettings& Settings,
		const FCameraState& CameraState,
		const FVector& CameraLocation)
	{
		FCameraDepthViewData Data;
		Data.bEnableDOF = Settings.DOFSettings.bEnableDOF
			&& Settings.FocusSettings.FocusMode != ECameraFocusMode::CFM_None;
		Data.bUseSceneDepth = Settings.SceneDepthDesc.bUseSceneDepth || Data.bEnableDOF;
		Data.bLinearizeDepth = Settings.SceneDepthDesc.bLinearizeDepth;
		Data.DepthSpace = Settings.SceneDepthDesc.DepthSpace;
		Data.NearPlane = CameraState.NearZ;
		Data.FarPlane = CameraState.FarZ;
		Data.FocalLength = Settings.DOFSettings.FocalLength;
		Data.FocusRange = Settings.DOFSettings.FocusRange;
		Data.Aperture = Settings.DOFSettings.Aperture;
		Data.MaxCoCRadius = Settings.DOFSettings.MaxBlurRadius;
		Data.MaxNearCoCRadius = Settings.DOFSettings.MaxBlurRadius;
		Data.MaxFarCoCRadius = Settings.DOFSettings.MaxBlurRadius;
		Data.CurrentFocusDistance = ResolveFocusDistance(Settings, CameraLocation);
		Data.DebugView = Settings.DOFSettings.DebugView;
		return Data;
	}
}

void UCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// E.2/3: PC 가 BeginPlay 시점엔 아직 spawn 전 → PlayerCameraManager nullptr.
	// PC 의 BeginPlay 에서 World 의 모든 카메라 컴포넌트를 catch up 등록하므로 안전.
	if (UWorld* World = GetOwner()->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
			{
				CM->RegisterCamera(this);
			}
		}
	}
}

void UCameraComponent::EndPlay()
{
	Super::EndPlay();
	if (UWorld* World = GetOwner()->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
			{
				CM->UnregisterCamera(this);
			}
		}
	}
}

void UCameraComponent::LookAt(const FVector& Target)
{
	FVector Position = GetWorldLocation();
	FVector Diff = (Target - Position).Normalized();

	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	FRotator LookRotation = GetRelativeRotation();
	LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;

	if (fabsf(Diff.Z) < 0.999f) {
		LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
	}

	SetRelativeRotation(LookRotation);
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
	CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
	CameraState = NewState;
}

void UCameraComponent::GetCameraView(float /*DeltaTime*/, FMinimalViewInfo& OutPOV) const
{
	UpdateWorldMatrix();
	const FVector CameraLocation = GetWorldLocation();
	OutPOV.Location    = CameraLocation;
	OutPOV.Rotation    = GetWorldMatrix().ToRotator();
	OutPOV.FOV         = CameraState.FOV;
	OutPOV.AspectRatio = CameraState.AspectRatio;
	OutPOV.OrthoWidth  = CameraState.OrthoWidth;
	OutPOV.NearClip    = CameraState.NearZ;
	OutPOV.FarClip     = CameraState.FarZ;
	OutPOV.bIsOrtho    = CameraState.bIsOrthogonal;
	OutPOV.DepthViewData = ResolveCameraDepthViewData(CameraViewSettings, CameraState, CameraLocation);
}
