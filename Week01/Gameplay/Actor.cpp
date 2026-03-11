#include "pch.h"
#include "Actor.h"
#include "Stage.h"

void FActor::Update(float DeltaTime, FGameContext &Context)
{
    if (bIsMoving)
    {
        MoveTimer += DeltaTime;
        float Alpha = MoveTimer / MoveDuration;

        if (Alpha >= 1.0f)
        {
            Alpha = 1.0f;
            bIsMoving = false; // 이동 완료
        }

        // 시작 지점에서 목표 지점까지 선형 보간 (Lerp)
        RenderX = MoveStartX + (MoveTargetX - MoveStartX) * Alpha;
        RenderY = MoveStartY + (MoveTargetY - MoveStartY) * Alpha;
    }

    Animator.Update(DeltaTime);
}

void FActor::SetPosition(int InTileX, int InTileY, float TileSize)
{
    TileX = InTileX;
    TileY = InTileY;
    RenderX = static_cast<float>(TileX) * TileSize;
    RenderY = static_cast<float>(TileY) * TileSize;
}

bool FActor::TryMove(FStage &Stage, EDirection Dir)
{
    int NextX = TileX;
    int NextY = TileY;

    switch (Dir)
    {
    case EDirection::Up:
        NextY--;
        break;
    case EDirection::Down:
        NextY++;
        break;
    case EDirection::Left:
        NextX--;
        break;
    case EDirection::Right:
        NextX++;
        break;
    }

    Direction = Dir;

    if (!Stage.CanMoveTo(NextX, NextY))
    {
        return false;
    }

    StartMoveTo(NextX, NextY, Stage.GetTileSize());
    return true;
}

void FActor::StartMoveTo(int InTileX, int InTileY, float TileSize)
{
    MoveStartX = RenderX;
    MoveStartY = RenderY;
    MoveTargetX = static_cast<float>(InTileX) * TileSize;
    MoveTargetY = static_cast<float>(InTileY) * TileSize;

    TileX = InTileX;
    TileY = InTileY;

    MoveTimer = 0.0f;
    bIsMoving = true;
}

int FActor::GetTileX() const { return TileX; }

int FActor::GetTileY() const { return TileY; }

float FActor::GetRenderX() const { return RenderX; }

float FActor::GetRenderY() const { return RenderY; }

void FActor::SetDirection(EDirection InDirection) { Direction = InDirection; }

EDirection FActor::GetDirection() const { return Direction; }

void FActor::SetHp(int InHp) { Hp = InHp; }

int FActor::GetHp() const { return Hp; }

void FActor::Damage(int Amount)
{
    if (bIsInvincible)
        return;
    Hp -= Amount;
    if (Hp < 0)
        Hp = 0;
}

void FActor::SetInvincible(bool bEnable) { bIsInvincible = bEnable; }

bool FActor::IsInvincible() const { return bIsInvincible; }

void FActor::Heal(int Amount) { Hp += Amount; }

bool FActor::IsDead() const { return Hp <= 0; }

bool FActor::IsMoving() const { return bIsMoving; }

void FActor::SetSprite(const FSpriteInfo &InSprite) { Sprite = InSprite; }

const FSpriteInfo &FActor::GetSprite() const
{
    // Animator가 활성 상태면 현재 프레임의 UV 영역을 반영
    if (Animator.IsPlaying())
    {
        const FKeyframe *KF = Animator.GetCurrentKeyframe();
        if (KF)
        {
            FSpriteInfo &MutableSprite = const_cast<FSpriteInfo &>(Sprite);
            MutableSprite.SpriteOffset = KF->OffsetMin;
            MutableSprite.SpriteSize = {KF->OffsetMax.x - KF->OffsetMin.x,
                                        KF->OffsetMax.y - KF->OffsetMin.y};
        }
    }
    return Sprite;
}

FSpriteAnimator &FActor::GetAnimator() { return Animator; }

const FSpriteAnimator &FActor::GetAnimator() const { return Animator; }

void FActor::SetDefaultMirrored(bool bMirrored) { bDefaultMirrored = bMirrored; }

bool FActor::GetDefaultMirrored() const { return bDefaultMirrored; }
