#include "CameraModifier.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_ABSTRACT_CLASS(UCameraModifier, UObject)

UCameraModifier::UCameraModifier()
{

}

void UCameraModifier::DisableModifier(bool bImmediate) {
	if (bImmediate) bDisabled = true; return;
	bPendingDisable = true;
}