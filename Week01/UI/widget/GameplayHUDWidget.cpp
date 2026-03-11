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

    if (ImGui::Begin("##HUD", nullptr, Flags))
    {
        float Remaining = Stage->GetRemainingTime();
        int   Minutes = static_cast<int>(Remaining) / 60;
        int   Seconds = static_cast<int>(Remaining) % 60;
        int   IRemaining = static_cast<int>(Remaining);

        ImGui::Text("HP    %d", HP);
        if (Remaining <= 10.0f)
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Time  %d:%02d", Minutes, Seconds);
        else
            ImGui::Text("Time  %d", Minutes, Seconds);
        ImGui::Text("Score %d", Score);
        if (Combo > 0)
        {
            ImGui::Text("Combo x%d", Combo);
        }

        ImGui::Separator();
        if (PauseFlag)
        {
            if (ImGui::Button(bPaused ? "Resume" : "Pause", ImVec2(80, 0)))
            {
                *PauseFlag = !*PauseFlag;
            }
        }
    }
    ImGui::End();

    // 비트맵 폰트 렌더링
    if (FontTexPair *Pair = Context.FontManager.Get("basic_font"))
    {
        FBitmapFont *Font = Pair->Font.get();
        FTexture    *FontTex = Pair->Tex.get();

        int Minutes = static_cast<int>(ReaminingTime) / 60;
        int Seconds = static_cast<int>(ReaminingTime) % 60;
        int IRemaining = static_cast<int>(ReaminingTime);

        char TimeBuf[32];
        // snprintf(TimeBuf, sizeof(TimeBuf), "Time %d:%02d", Minutes, Seconds);
        snprintf(TimeBuf, sizeof(TimeBuf), "Time %03d", IRemaining);

        char ScoreBuf[32];
        snprintf(ScoreBuf, sizeof(ScoreBuf), "Score %d", Score);

        Context.Renderer.DrawFont(ScoreBuf, Font, FontTex, 30, HPTextPos.Y + 50, 50);
        Context.Renderer.DrawFont(TimeBuf, Font, FontTex, HPTextPos.X + 100, HPTextPos.Y + 50, 50);
        std::string StageName = Stage->GetStageName();
        auto        iter = StageName.find(':');
        std::string StageIdx = StageName.substr(0, iter);
        Context.Renderer.DrawFont(StageIdx, Font, FontTex, 30, HPTextPos.Y, 50);
        // Context.Renderer.DrawFont("HP", Font, FontTex, HPTextPos.X, HPTextPos.Y, 35);
    }
    for (int i = 0; i < HP; i++)
    {
        float xPos = 980.f - (HeartScale * i);
        Context.Renderer.DrawTexture(LifeTexture, xPos, HPTextPos.Y + 30, HeartScale, HeartScale);
    }

    // 버프 아이콘 (우상단, 셰이더 렌더링)
    {
        float       ScreenW = static_cast<float>(Context.Renderer.GetScreenWidth());
        const float IconSize = 40.0f;
        const float IconPadding = 8.0f;
        const float MarginRight = 20.f;
        const float MarginTop = 960.0f;
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
            case EItemType::TimeScaleUp:
                snprintf(Buf, sizeof(Buf), "%.1fs", Eff.RemainingTime);
                Buffs.push_back({GetItemTextureKey(Eff.Type), Buf});
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
