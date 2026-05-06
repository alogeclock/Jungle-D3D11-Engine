#include "CameraModifier_CameraShake.h"
#include "PlayerCameraManager.h"

IMPLEMENT_CLASS(UCameraModifier_CameraShake, UCameraModifier)

bool UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, UCameraComponent& InOutPOV) {
	
	return false;
}

void UCameraModifier_CameraShake::UpdateAlpha(float DeltaTime) {}