#include "pch.h"
#include "BeatEffect.h"
#include "Render/Renderer.h"
#include "Render/FontManager.h"
#include <string>
void BeatEffect::Update(float DeltaTime)
{
    LifeTime -= DeltaTime;
    Y -= Speed * DeltaTime;
}

bool BeatEffect::IsAlive() const { return LifeTime >= 0.f; }

void BeatEffect::Reset(FTexture *InTexture, float StartX, float StartY, float inLifeTime,
                       float InScore)
{
    Texture = InTexture;
    X = StartX;
    Y = StartY;
    LifeTime = inLifeTime;
    Score = InScore;
}

void BeatEffect::Render(FGameContext &Context)
{
    if (IsAlive())
    {
        FontTexPair *FTPair = Context.FontManager.Get("basic_font");
        Context.Renderer.DrawTexture(Texture, X, Y, 200, 100);
        Context.Renderer.DrawFont("+" + std::to_string(static_cast<int>(Score)), FTPair->Font.get(),
                                  FTPair->Tex.get(), X - 20, Y - 50, 40);
    }
}
