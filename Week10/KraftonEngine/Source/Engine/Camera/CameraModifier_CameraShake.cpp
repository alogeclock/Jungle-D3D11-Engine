#include "CameraModifier_CameraShake.h"
#include "PlayerCameraManager.h"

IMPLEMENT_CLASS(UCameraModifier_CameraShake, UCameraModifier)

void UCameraModifier_CameraShake::AddedToCamera(APlayerCameraManager* Camera) {}

bool UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) {
	return false;
}

void UCameraModifier_CameraShake::UpdateAlpha(float DeltaTime) {}