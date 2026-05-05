#pragma once
#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/PlayerController.h"

/***********************************************
***********************************************/
class APawnActor;
class UCameraModifier;

struct FViewTarget
{
	AActor* Target;

public:
	void SetNewTarget(AActor* NewTarget);

	APawnActor* GetTargetPawn() const;
		
	bool Equal(const FViewTarget& OtherTarget) const;

	FViewTarget() : Target(nullptr) { }

	// ViewTarget이 Valid함을 보장하는 함수
	void CheckViewTarget(APlayerController* OwningController);
};

class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)


	// TODO: 해당 프로젝트에 FLinearColor없어서 임시로 FColor 박아둠 -> 추후 LinearColor로 변경할 것
	// 이 프로젝트에서는 FColor를 ToVector4로 변환해서 사용
	FColor FadeColor;
	float FadeAmount;
	FVector2 FadeAlpha;
	float FadeTime;
	float FadeTimeRemaining;

	FName CameraStyle;
	FViewTarget ViewTarget;

	TArray<UCameraModifier*> ModifierList;
};

