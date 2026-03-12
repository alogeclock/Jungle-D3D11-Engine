#include "pch.h"
#include "GameplayHUDWidget.h"
#include "Gameplay/Stage.h"
#include "Gameplay/Item.h"
#include "Core/Time.h"
#include "Core/Logger.h"
#include "Render/Renderer.h"
#include "Render/FontManager.h"
#include "Render/Texture.h"
#include "Render/TextureManager.h"
#include <algorithm>
#include <cmath>

void FGameplayHUDWidget::BindStage(const FStage *InStage) { Stage = InStage; }

void FGameplayHUDWidget::BindPauseFlag(bool *InPauseFlag) { PauseFlag = InPauseFlag; }

void FGameplayHUDWidget::ResetPlayTime() { PlayTime = 0.0f; }

void FGameplayHUDWidget::Update(FGameContext &Context)
{
    bool bPaused = PauseFlag && *PauseFlag;
    if (!bPaused)
    {
        PlayTime += Context.Time.GetDeltaTime();
    }

    // Score 애니메이션 업데이트
    if (Stage)
    {

        if (DisplayScore != TargetScore)
        {
            ScoreAnimTimer += Context.Time.GetDeltaTime();
            float T = ScoreAnimTimer / ScoreAnimDuration;
            if (T >= 1.0f)
            {
                T = 1.0f;
                DisplayScore = TargetScore;
            }
            else
            {
                DisplayScore =
                    ScoreAnimStart + static_cast<int>((TargetScore - ScoreAnimStart) * T);
            }
        }
    }
}

void FGameplayHUDWidget::Render(FGameContext &Context)
{
    if (!Stage)
        return;

    bool bPaused = PauseFlag && *PauseFlag;

    int   Score = Stage->GetScoreSystem().GetScore();
    int   Combo = Stage->GetScoreSystem().GetCombo();
    float ReaminingTime = Stage->GetRemainingTime();
    // int StageIdx = Stage->Get
    int HP = Stage->GetPlayer().GetHp();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);

    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

#ifdef _DEBUG
    if (ImGui::Begin("##HUD", nullptr, Flags))
    {
        if (PauseFlag)
        {
            if (ImGui::Button(bPaused ? "Resume" : "Pause", ImVec2(80, 0)))
            {
                *PauseFlag = !*PauseFlag;
            }
        }
    }
    ImGui::End();
#endif

    float ScreenW = static_cast<float>(Context.Renderer.GetScreenWidth());
    float ScreenH = static_cast<float>(Context.Renderer.GetScreenHeight());

    const float MarginRight = 40.0f;
    const float TopY = 20.0f;

    // 비트맵 폰트 렌더링
    if (FontTexPair *Pair = Context.FontManager.Get("basic_font"))
    {
        FBitmapFont *Font = Pair->Font.get();
        FTexture    *FontTex = Pair->Tex.get();

        // 텍스트 픽셀 폭 계산 람다
        auto MeasureText = [&](const char *Text, float Scale) -> float
        {
            float Width = 0.0f;
            float ScaleF = Scale / static_cast<float>(Font->LineHeight);
            for (const char *C = Text; *C; ++C)
            {
                const FGlyph *G = Font->Get(static_cast<int>(*C));
                if (G)
                    Width += G->XAdvance * ScaleF;
            }
            return Width;
        };

        int IRemaining = static_cast<int>(ReaminingTime);

        char TimeBuf[32];
        snprintf(TimeBuf, sizeof(TimeBuf), "Time   %03d", IRemaining);

        char ScoreBuf[32];
        snprintf(ScoreBuf, sizeof(ScoreBuf), "Score %05d", DisplayScore);

        // Score: 우측 정렬
        float ScoreFontSize = 50.0f;
        float ScoreTextW = MeasureText(ScoreBuf, ScoreFontSize);
        float ScoreX = ScreenW - MarginRight - ScoreTextW;
        Context.Renderer.DrawFont(ScoreBuf, Font, FontTex, ScoreX, TopY + 120, ScoreFontSize);

        // Time: 우측 정렬
        float TimeScale = 50.0f;
        if (Stage->IsAngry())
        {
            float Pulse = (sinf(PlayTime * 2.0f * 3.14159f) + 1.0f) * 0.5f;
            TimeScale = 50.0f * (1.0f + 0.15f * Pulse);
        }
        float TimeTextW = MeasureText(TimeBuf, TimeScale);
        float TimeX = ScreenW - MarginRight - TimeTextW;
        Context.Renderer.DrawFont(TimeBuf, Font, FontTex, TimeX, TopY + 70, TimeScale);

        // Stage 배경
        FTexture *StageBackGroundTexture = Context.Textures.Get("stage_back");
        if (StageBackGroundTexture)
            Context.Renderer.DrawTexture(StageBackGroundTexture, 125, TopY + 5, 230, 65);

        // Stage 이름: 좌측 고정
        std::string StageName = Stage->GetStageName();
        auto        iter = StageName.find(':');
        std::string StageIdx = StageName.substr(0, iter);
        Context.Renderer.DrawFont(StageIdx, Font, FontTex, 50, TopY, 50);
    }

    for (int i = 0; i < HP; i++)
    {
        float xPos = (ScreenW - MarginRight) - (HeartScale * i);
        Context.Renderer.DrawTexture(LifeTexture, xPos - HeartScale / 2.0f, TopY + 30, HeartScale,
                                     HeartScale);
    }

    // 버프 아이콘 (우상단, 셰이더 렌더링)
    {
        const float IconSize = 40.0f;
        const float IconPadding = 8.0f;
        const float MarginRight = 20.f;
        const float MarginTop = ScreenH - 60.0f;
        const float FontSize = 20.0f;

        struct FBuffDisplay
        {
            std::string TexKey;
            std::string Label;
        };
        std::vector<FBuffDisplay> Buffs;

        const auto &PlayerEffects = Stage->GetPlayer().GetActiveEffects();
        for (const auto &Eff : PlayerEffects)
        {
            char Buf[32];
            switch (Eff.Type)
            {
            case EItemType::Invincibility:
                Buffs.push_back({GetItemTextureKey(Eff.Type), "INV"});
                break;
            case EItemType::TimeScaleDown:
                snprintf(Buf, sizeof(Buf), "%.1fs", Eff.RemainingTime);
                Buffs.push_back({GetItemTextureKey(Eff.Type), Buf});
                break;
            default:
                break;
            }
        }

        if (Stage->IsTimeFrozen())
        {
            char Buf[32];
            snprintf(Buf, sizeof(Buf), "%.1fs", Stage->GetTimeFreezeRemaining());
            Buffs.push_back({"item_time_freeze", Buf});
        }

        for (int i = 0; i < static_cast<int>(Buffs.size()); ++i)
        {
            FTexture *Tex = Context.Textures.Get(Buffs[i].TexKey);
            if (!Tex)
                continue;

            float X = MarginRight + IconSize * 0.5f + (IconSize + IconPadding) * i;
            float Y = MarginTop + IconSize * 0.5f;

            Context.Renderer.DrawTexture(Tex, X, Y, IconSize, IconSize);

            if (FontTexPair *Pair = Context.FontManager.Get("basic_font"))
            {
                float TextX = X - FontSize * Buffs[i].Label.size() * 0.25f;
                float TextY = Y + IconSize * 0.5f + 4.0f;
                Context.Renderer.DrawFont(Buffs[i].Label, Pair->Font.get(), Pair->Tex.get(), TextX,
                                          TextY, FontSize);
            }
        }
    }

    // 일시정지 오버레이
    if (bPaused)
    {
        float ScreenW = static_cast<float>(Context.Renderer.GetScreenWidth());
        float ScreenH = static_cast<float>(Context.Renderer.GetScreenHeight());

        ImGui::SetNextWindowPos(ImVec2(ScreenW * 0.5f, ScreenH * 0.5f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.7f);

        ImGuiWindowFlags PauseFlags = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("##PauseOverlay", nullptr, PauseFlags))
        {
            ImGui::Text("PAUSED");
            ImGui::Separator();
            if (ImGui::Button("Resume", ImVec2(120, 0)))
            {
                *PauseFlag = false;
            }
        }
        ImGui::End();
    }
}

void FGameplayHUDWidget::SetTextures(FGameContext &Context)
{
    if (!LifeTexture)
    {
        LifeTexture = Context.Textures.Get("life_heart");
    }
    if (!LifeDeadTexture)
    {
        LifeDeadTexture = Context.Textures.Get("life_dead");
    }
}

void FGameplayHUDWidget::OnBeatScoreUpdate(int InScore)
{
    DisplayScore = InScore;
    TargetScore = DisplayScore;
}

void FGameplayHUDWidget::OnTimerBonusUpdate(int InScore)
{
    int ActualScore = InScore;
    if (ActualScore != TargetScore)
    {
        ScoreAnimStart = DisplayScore;
        TargetScore = ActualScore;
        ScoreAnimTimer = 0.0f;
    }
}
