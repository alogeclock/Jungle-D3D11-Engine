#pragma once
#include "CameraModifier.h"

class UCameraModifier_CameraShake : public UCameraModifier {
public:
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

	void AddedToCamera(APlayerCameraManager* Camera);
	bool ModifyCamera(float DeltaTime, UCameraComponent& InOutPOV);
	void UpdateAlpha(float DeltaTime);

private:


};