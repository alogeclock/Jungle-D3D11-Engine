#pragma once

#include "Core/CoreTypes.h"
#include "Object/Object.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
public:
	DECLARE_CLASS(UCameraModifier, UObject)
	UCameraModifier();

	// Unreal Engine도 Public에 넣어둠
	uint8 Priority;

private:
	APlayerCameraManager* CameraOwner;
	float AlphaInTime;
	float AlphaOutTime;
	float Alpha;
	uint32 bDisabled;
};

