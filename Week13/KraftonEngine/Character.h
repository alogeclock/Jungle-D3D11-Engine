#pragma once
#include "GameFramework/Pawn.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UCharacterMovementComponent;

class ACharacter : public APawn
{
public:
	DECLARE_CLASS(ACharacter, APawn)

	ACharacter();
	~ACharacter() override = default;

	void Serialize(FArchive& Ar) override;

	// ── 이동 입력 (PlayerController가 호출) ──────────────────
	void AddMovementInput(FVector WorldDir, float Scale = 1.0f);
	void AddControllerYawInput(float Val);   // 좌우 회전
	void AddControllerPitchInput(float Val); // 상하 시선

	// ── 점프 ─────────────────────────────────────────────────
	virtual void Jump();
	virtual void StopJumping();
	bool CanJump() const;

	// ── 앉기 ─────────────────────────────────────────────────
	virtual void Crouch();
	virtual void UnCrouch();

	// ── 외부 속도 부여 ────────────────────────────────────────
	void LaunchCharacter(FVector LaunchVelocity, bool bOverrideXY, bool bOverrideZ);

	// ── 상태 조회 ─────────────────────────────────────────────
	bool IsJumping() const;
	bool IsCrouching() const;
	bool IsFalling() const;
	bool IsOnGround() const;

	// ── 컴포넌트 접근자 ───────────────────────────────────────
	UCapsuleComponent* GetCapsuleComponent()  const { return CapsuleComponent; }
	USkeletalMeshComponent* GetMesh()              const { return Mesh; }
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

protected:
	// Pawn override — Possess 시 입력 바인딩
	void PossessedBy(APlayerController* PC) override;
	void UnPossessed() override;

	// 자식 클래스용 이벤트 알림
	virtual void OnJumped() {}
	virtual void OnLanded(const FVector& HitNormal) {}
	virtual void OnStartCrouch(float HalfHeightAdjust) {}
	virtual void OnEndCrouch(float HalfHeightAdjust) {}

private:
	UCapsuleComponent* CapsuleComponent = nullptr;
	USkeletalMeshComponent* Mesh = nullptr;
	UCharacterMovementComponent* CharacterMovement = nullptr;

	bool bPressedJump = false;
	float JumpKeyHoldTime = 0.0f;
};
