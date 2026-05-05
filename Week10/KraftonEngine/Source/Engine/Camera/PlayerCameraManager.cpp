#include "PlayerCameraManager.h"

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

void FViewTarget::SetNewTarget(AActor* NewTarget)
{

}

APawnActor* FViewTarget::GetTargetPawn() const
{
	return nullptr;
}

bool FViewTarget::Equal(const FViewTarget& OtherTarget) const
{
	return false;
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{

}
