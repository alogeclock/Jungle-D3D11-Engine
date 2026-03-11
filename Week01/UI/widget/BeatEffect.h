#pragma once
#include "Core/GameContext.h"
#include "Render/Texture.h"
struct BeatEffect
{
    BeatEffect() {}
    BeatEffect(FTexture *InTexture, float StartX, float StartY)
        : Texture(InTexture), X(StartX), Y(StartY)
    {
    }
    void Update(float DeltaTime);
    bool IsAlive() const;
    void Reset(FTexture *InTexture, float StartX, float StartY, float inLifeTime);
    void Render(FGameContext &Context);

    const FTexture *Texture = nullptr;
    float           X = 0.f;
    float           Y = 0.f;
    float           LifeTime = -1.f;

  private:
    float Speed = 100.f;
};