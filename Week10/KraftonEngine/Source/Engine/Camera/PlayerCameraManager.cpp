#include "PlayerCameraManager.h"
#include "Camera/LuaCameraModifier.h"
#include "Engine/GameFramework/PlayerController.h"
#include "Engine/GameFramework/PawnActor.h"
#include "Object/Object.h"
#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"

#include <algorithm>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

void FViewTarget::SetNewTarget(AActor* NewTarget)
{
	if (!NewTarget)
	{
		return;
	}

	Target = NewTarget;
}

APawnActor* FViewTarget::GetTargetPawn() const
{
	if (!Target)
	{
		return nullptr;
	}

	return Cast<APawnActor>(Target);
}

bool FViewTarget::Equal(const FViewTarget& OtherTarget) const
{
	return Target != nullptr && Target == OtherTarget.Target;
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{
	if (!Target && OwningController)
	{
		Target = OwningController->GetPawn();
	}
}

void APlayerCameraManager::BeginPlay()
{
	AActor::BeginPlay();
}

void APlayerCameraManager::EndPlay()
{
	ModifierList.clear();
	AActor::EndPlay();
}

void APlayerCameraManager::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	UpdateCamera(DeltaTime);
}

// Function : Add camera modifier to manager and sort by priority
// input : InModifier
// InModifier : modifier instance that will edit final camera POV
void APlayerCameraManager::AddCameraModifier(UCameraModifier* InModifier)
{
	if (!InModifier)
	{
		return;
	}

	for (UCameraModifier* ExistingModifier : ModifierList)
	{
		if (ExistingModifier == InModifier)
		{
			InModifier->EnableModifier();
			return;
		}
	}

	InModifier->AddedToCamera(this);
	InModifier->EnableModifier();
	ModifierList.push_back(InModifier);
	SortModifiersByPriority();
}

// Function : Create and play a Lua camera modifier through PlayerCameraManager
// input : ScriptPath, Params
// ScriptPath : Lua modifier script path under Scripts
// Params : numeric parameters passed to Lua modifier Begin(params)
void APlayerCameraManager::PlayCameraModifier(const FString& ScriptPath, const TMap<FString, float>& Params)
{
	ULuaCameraModifier* Modifier = UObjectManager::Get().CreateObject<ULuaCameraModifier>(this);
	if (!Modifier)
	{
		return;
	}

	if (!Modifier->Initialize(ScriptPath, Params))
	{
		UObjectManager::Get().DestroyObject(Modifier);
		return;
	}

	AddCameraModifier(Modifier);
}

// Function : Apply active camera modifiers to the current POV
// input : DeltaTime, InOutPOV
// DeltaTime : frame delta time
// InOutPOV : camera POV modified by active modifiers in priority order
void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	RemoveFinishedModifiers();

	for (UCameraModifier* CameraModifier : ModifierList)
	{
		if (!CameraModifier || CameraModifier->IsDisabled())
		{
			continue;
		}

		CameraModifier->UpdateAlpha(DeltaTime);
		if (CameraModifier->IsDisabled())
		{
			continue;
		}

		const bool bStopProcessing = CameraModifier->ModifyCamera(DeltaTime, InOutPOV);
		if (bStopProcessing)
		{
			break;
		}
	}

	RemoveFinishedModifiers();
}

// Function : Update view target POV and camera cache for this frame
// input : DeltaTime
// DeltaTime : frame delta time used by CalcCamera and modifiers
void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	LastFrameCameraCache = CameraCache;

	ViewTarget.CheckViewTarget(Owner);

	FMinimalViewInfo NewPOV;
	if (ViewTarget.Target)
	{
		ViewTarget.Target->CalcCamera(DeltaTime, NewPOV);
	}
	else
	{
		NewPOV = BuildFallbackCameraView(nullptr);
	}

	ApplyCameraModifiers(DeltaTime, NewPOV);

	ViewTarget.POV = NewPOV;
	CameraCache.TimeStamp += DeltaTime;
	CameraCache.POV = NewPOV;
}

// Function : Sort active camera modifiers by priority
// input : none
// Priority : modifier with higher priority is processed first
void APlayerCameraManager::SortModifiersByPriority()
{
	std::sort(ModifierList.begin(), ModifierList.end(),
		[](const UCameraModifier* A, const UCameraModifier* B)
		{
			if (!A)
			{
				return false;
			}
			if (!B)
			{
				return true;
			}
			return A->Priority > B->Priority;
		});
}

// Function : Remove modifiers that finished or became invalid
// input : none
// ModifierList : active modifier list owned by PlayerCameraManager
void APlayerCameraManager::RemoveFinishedModifiers()
{
	ModifierList.erase(
		std::remove_if(ModifierList.begin(), ModifierList.end(),
			[](UCameraModifier* Modifier)
			{
				return !Modifier || Modifier->IsFinished();
			}),
		ModifierList.end());
}

UCameraComponent* APlayerCameraManager::FindCameraComponent(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}

	return Target->GetComponentByClass<UCameraComponent>();
}

FMinimalViewInfo APlayerCameraManager::BuildFallbackCameraView(AActor* Target) const
{
	FMinimalViewInfo ViewInfo;
	if (Target)
	{
		ViewInfo.Location = Target->GetActorLocation();
		ViewInfo.Rotation = Target->GetActorRotation();
	}
	else
	{
		ViewInfo.Location = FVector::ZeroVector;
		ViewInfo.Rotation = FRotator::ZeroRotator;
	}

	ViewInfo.FOV = 3.14159265358979f / 3.0f;
	ViewInfo.NearZ = 0.1f;
	ViewInfo.FarZ = 1000.0f;
	ViewInfo.bIsOrthogonal = false;
	ViewInfo.OrthoWidth = 10.0f;
	ViewInfo.PostProcessBlendWeight = 1.0f;

	return ViewInfo;
}
