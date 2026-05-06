#pragma once
#include "CameraModifier.h"

struct FCameraShakeState {

};

class UCameraModifier_CameraShake : public UCameraModifier {
public:
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

	bool ModifyCamera(float DeltaTime, UCameraComponent& InOutPOV);
	void UpdateAlpha(float DeltaTime);

private:


};