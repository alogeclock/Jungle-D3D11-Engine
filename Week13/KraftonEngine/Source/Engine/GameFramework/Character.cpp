#include "Character.h"
#include "GameFramework/PlayerController.h"
#include "Serialization/Archive.h"
#include <Component/CameraComponent.h>

ACharacter::ACharacter()
{
	
}

void ACharacter::InitDefaultComponents()
{
	CapsuleComponent = AddComponent<UCapsuleComponent>();
	SetRootComponent(CapsuleComponent);

	Mesh = AddComponent<USkeletalMeshComponent>();
	GetRootComponent()->AddChild(Mesh);

	Camera = AddComponent<UCameraComponent>();
	GetRootComponent()->AddChild(Camera);

	Camera->SetRelativeLocation(FVector(-10.f, 0.f, 10.f));
	Camera->SetRelativeRotation(FVector(0.f, 45.f, 0.f));

	CharacterMovement = AddComponent<UCharacterMovementComponent>();
	CharacterMovement->SetUpdatedComponent(GetRootComponent());

	LuaScript = AddComponent<ULuaScriptComponent>();
	LuaScript->SetScriptFile("PlayerController.lua");
}

void ACharacter::Serialize(FArchive& Ar)
{
	APawn::Serialize(Ar);
}


void ACharacter::PostDuplicate()
{
	CapsuleComponent = Cast<UCapsuleComponent>(GetRootComponent());
	if (!CapsuleComponent)
	{
		CapsuleComponent = GetComponentByClass<UCapsuleComponent>();
	}

	Mesh = GetComponentByClass<USkeletalMeshComponent>();
	Camera = GetComponentByClass<UCameraComponent>();
	CharacterMovement = GetComponentByClass<UCharacterMovementComponent>();
	LuaScript = GetComponentByClass<ULuaScriptComponent>();

	if (CharacterMovement && CapsuleComponent)
	{
		CharacterMovement->SetUpdatedComponent(CapsuleComponent);
	}

	if (LuaScript && LuaScript->GetScriptFile().empty())
	{
		LuaScript->SetScriptFile("PlayerController.lua");
	}
}

void ACharacter::PossessedBy(APlayerController* PC)
{
	APawn::PossessedBy(PC);
}

void ACharacter::UnPossessed()
{
	APawn::UnPossessed();
}

// ── 이동 입력 ─────────────────────────────────────────────────

void ACharacter::AddMovementInput(FVector WorldDir, float Scale)
{
	if (!CharacterMovement)
	{
		return;
	}
	CharacterMovement->AddMoveInput(WorldDir.X * Scale, WorldDir.Y * Scale);
}

void ACharacter::AddControllerYawInput(float Val)
{
	if (!CharacterMovement)
	{
		return;
	}
	CharacterMovement->AddLookInput(Val, 0.0f);
}

void ACharacter::AddControllerPitchInput(float Val)
{
	if (!CharacterMovement)
	{
		return;
	}
	CharacterMovement->AddLookInput(0.0f, Val);
}

// ── 점프 ─────────────────────────────────────────────────────

void ACharacter::Jump()
{
	if (!CanJump())
	{
		return;
	}
	bPressedJump = true;
	CharacterMovement->Jump();
	OnJumped();
}

void ACharacter::StopJumping()
{
	bPressedJump = false;
	JumpKeyHoldTime = 0.0f;
}

bool ACharacter::CanJump() const
{
	return CharacterMovement && CharacterMovement->IsMovingOnGround();
}

// ── 상태 조회 ─────────────────────────────────────────────────

bool ACharacter::IsJumping() const
{
	return bPressedJump && IsFalling();
}

bool ACharacter::IsFalling() const
{
	return CharacterMovement && CharacterMovement->IsFalling();
}

bool ACharacter::IsOnGround() const
{
	return CharacterMovement && CharacterMovement->IsMovingOnGround();
}
