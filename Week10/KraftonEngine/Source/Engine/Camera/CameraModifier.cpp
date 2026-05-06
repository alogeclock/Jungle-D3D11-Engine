#include "CameraModifier.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_ABSTRACT_CLASS(UCameraModifier, UObject)

UCameraModifier::UCameraModifier()
{

}

void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraManager) {
	if (!InCameraManager) CameraOwner = InCameraManager;
}

void UCameraModifier::DisableModifier(bool bImmediate) {
	if (bImmediate) bDisabled = true; return;
	bPendingDisable = true;
}
