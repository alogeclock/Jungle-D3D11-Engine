#pragma once
#include "CameraModifier.h"

struct FCameraShakeState {

};

class UCameraModifier_CameraShake : public UCameraModifier {
public:
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

	void AddedToCamera(APlayerCameraManager* Camera) override;
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	void UpdateAlpha(float DeltaTime) override;

private:


};