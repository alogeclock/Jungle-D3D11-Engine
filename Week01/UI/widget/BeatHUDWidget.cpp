#include "pch.h"
#include "BeatHUDWidget.h"
#include "Core/Time.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include "Render/FontManager.h"
#include <cmath>
#include "Core/Logger.h"

void FBeatHUDWidget::BindBeatSystem(const FBeatSystem *InBeatSystem)
{
    BeatSystem = InBeatSystem;
    Ypos = 800.f;
    Heart.X = 500.f;
    Heart.Y = Ypos;

    HeartXScale = OrgHeartXScale;
    HeartYScale = OrgHeartYScale;
}

void FBeatHUDWidget::Update(FGameContext &Context)
{

    float DeltaTime = Context.Time.GetDeltaTime();

    for (BeatEffect &Effect : BeatEffects)
    {
        if (Effect.IsAlive())
        {
            Effect.Update(DeltaTime);
        }
    }
    float RecoverSpeed = 5.0f;
    HeartXScale += (OrgHeartXScale - HeartXScale) * RecoverSpeed * DeltaTime;
    HeartYScale += (OrgHeartYScale - HeartYScale) * RecoverSpeed * DeltaTime;

    // --- 추가: 색상 복구 로직 ---
    float ColorRecoverSpeed = 5.0f; // 스케일보다 약간 천천히 색상이 빠지게 연출
    HeartColorRatio += (0.0f - HeartColorRatio) * ColorRecoverSpeed * DeltaTime;
    if (HeartColorRatio < 0.0f)
        HeartColorRatio = 0.0f;
}

void FBeatHUDWidget::Render(FGameContext &Context)
{
    if (!BeatSystem)
        return;

    if (BeatSystem->GetBeatCount() == 0)
        return;

    // 화면 크기에 맞춰 위치 재계산
    float ScreenW = static_cast<float>(Context.Renderer.GetScreenWidth());
    float ScreenH = static_cast<float>(Context.Renderer.GetScreenHeight());
    Heart.X = ScreenW * 0.5f;
    Ypos = ScreenH * 0.8f;
    Heart.Y = Ypos;

    /*
     * 진행바랑 하트 렌더링
     */
    float BeatInterval = BeatSystem->GetBeatInterval();
    // 이전박자 ----- 현재 ----- 이후박자
    //     ElapsedTime   RemainedTime
    float RemainedTime = BeatSystem->GetTimeToNextBeat(); // 거리계산에 사용
    float ElapsedTime = BeatInterval - RemainedTime;
    float DistanceRatio = RemainedTime / BeatInterval;
    float Distance = MaxDistance * DistanceRatio;

    float LeftBarX = Heart.X - Distance;
    float RightBarX = Heart.X + Distance;

    float Margin = 20.0f;
    float AdormentXScale = Context.Renderer.GetScreenWidth() * 2 - Margin * 2.0f;

    Context.Renderer.DrawTexture(BarWidthTexture, Context.Renderer.GetScreenWidth() / 2.0f, Heart.Y,
                                 AdormentXScale, 20);
    Context.Renderer.DrawTexture(BarTexture, LeftBarX, Ypos, 20, 100);
    Context.Renderer.DrawTexture(BarTexture, RightBarX, Ypos, 20, 100);

    // --- 수정: 하트를 그릴 때 ColorTint 계산 후 전달 ---
    // (0,0,0,0) 이 No Tint 이므로 주황색(R:1.0, G:0.5, B:0.0)에 Ratio를 곱해줌
    DirectX::XMFLOAT4 HeartTint = {
        1.0f,                                // Red: 항상 1.0 유지
        1.0f - (0.5f * HeartColorRatio),     // Green: 1.0 -> 0.5 로 변화
        1.0f - (1.0f * HeartColorRatio),     // Blue: 1.0 -> 0.0 으로 변화
        HeartColorRatio > 0.0f ? 1.0f : 0.0f // Alpha (혹은 틴트 강도)
    };
    Context.Renderer.DrawTexture(HeartTexture, Heart.X, Heart.Y, HeartXScale, HeartYScale,
                                 HeartTint);

    FontTexPair *FTPair = Context.FontManager.Get("basic_font");
    if (Combo != 0)
        Context.Renderer.DrawFont("Combo x" + std::to_string(Combo), FTPair->Font.get(),
                                  FTPair->Tex.get(), Heart.X - 160, Heart.Y + 100, 75);
    /*
     * 이펙트 그리기
     */
    for (BeatEffect &Effect : BeatEffects)
    {
        if (Effect.IsAlive())
        {
            Effect.Render(Context);
        }
    }
}

void FBeatHUDWidget::SetTextures(FGameContext &Context)
{
    if (!BarTexture)
    {
        BarTexture = Context.Textures.Get("beat_bar");
    }
    if (!BarWidthTexture)
    {
        BarWidthTexture = Context.Textures.Get("beat_bar_width");
    }
    if (!HeartTexture)
    {
        HeartTexture = Context.Textures.Get("beat_heart");
    }
    if (!PerfectTexture)
    {
        PerfectTexture = Context.Textures.Get("effect_perfect");
    }
    if (!GoodTexture)
    {
        GoodTexture = Context.Textures.Get("effect_good");
    }
    if (!MissTexture)
    {
        MissTexture = Context.Textures.Get("effect_miss");
    }
}

void FBeatHUDWidget::OnBeatJudged(EBeatJudge Judge, float InScore, int InCombo)
{
    Combo = InCombo;
    switch (Judge)
    {
    case EBeatJudge::Perfect:
        GetFromPool(PerfectTexture, Heart.X, Heart.Y - 100, 0.3f, InScore, InCombo);
        HeartXScale = OrgHeartXScale * 1.5;
        HeartYScale = OrgHeartYScale * 1.4;
        HeartColorRatio = 1.0f; // <--- 추가: 주황색으로 번쩍임
        Logger::Log("Perfect! (in HUDWidget)");
        break;
    case EBeatJudge::Good:
        GetFromPool(GoodTexture, Heart.X, Heart.Y - 100, 0.3f, InScore, InCombo);
        HeartXScale = OrgHeartXScale * 1.25;
        HeartYScale = OrgHeartYScale * 1.2;
        HeartColorRatio = 1.0f; // <--- 추가: 주황색으로 번쩍임
        Logger::Log("Good! (in HUDWidget)");
        break;
    case EBeatJudge::Miss:
        GetFromPool(MissTexture, Heart.X, Heart.Y - 100, 0.3f, InScore, InCombo);
        HeartXScale = OrgHeartXScale * 0.9;
        HeartYScale = OrgHeartYScale * 0.95;
        Logger::Log("Miss! (in HUDWidget)");
        break;
    }
}

BeatEffect *FBeatHUDWidget::GetFromPool(FTexture *Texture, float PosX, float PosY, float InLifetime,
                                        float InScore, int InCombo)
{
    for (BeatEffect &Effect : BeatEffects)
    {
        if (!Effect.IsAlive())
        {
            Effect.Reset(Texture, PosX, PosY, InLifetime, InScore);
            return &Effect;
        }
    }
    return nullptr;
}
