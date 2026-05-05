#pragma once
#include "GameFramework/AActor.h"
#include "CameraModifier.h"
#include "Core/EngineTypes.h"

class APawnActor;
class APlayerController;

struct FViewTarget {
public:
	void SetNewTarget(AActor* InTarget);
	APawnActor* GetTargetPawn() const;
	bool Equal(const FViewTarget& OtherTarget) const;
	void CheckViewTarget(APlayerController* OwningController);

public:
	AActor*			  Target = nullptr;
	UCameraComponent* POV	 = nullptr;
};


class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)
	void ApplyCameraModifiers(float DeltaTime, UCameraComponent* InOutPOV);

public:
	FViewTarget		ViewTarget;
	FName			CameraStyle;
	FLinearColor	FadeColor;
	float			FadeAmount;
	FVector2		FadeAlpha;
	float			FadeTime;
	float			FadeTimeRemaining;

private:
	APlayerController* Owner = nullptr;
	TArray<UCameraModifier*> ModifierList;

};