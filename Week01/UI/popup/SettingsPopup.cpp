#include "pch.h"
#include "SettingsPopup.h"
#include "Core/AudioSystem.h"

void FSettingsPopup::LoadCurrentVolumes()
{
    MasterVolume = FAudioSystem::Get().GetMasterVolume();
    BgmVolume = FAudioSystem::Get().GetChannelVolume(EAudioChannel::BGM);
    SfxVolume = FAudioSystem::Get().GetChannelVolume(EAudioChannel::SFX);
}

EUIPopupAction FSettingsPopup::ConsumeAction()
{
    EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

void FSettingsPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("Settings", "Settings", ImVec2(500.0f, 340.0f), Layout))
    {
        return;
    }

    DrawVolumeSliders(Layout);

    if (DrawBottomButton(Layout, "Close"))
    {
        PendingAction = EUIPopupAction::ClosePopup;
    }

    EndPopupWindow();
}

void FSettingsPopup::DrawVolumeSliders(const FPopupFrameLayout &Layout)
{
    ImGui::SetWindowFontScale(GetContentFontScale(EUIPopupContentTextSize::Medium));

    const float SliderWidth = Layout.ContentWidth * 0.75f;
    const float LabelWidth = 100.0f;
    const float StartX = Layout.ContentLeft + (Layout.ContentWidth - SliderWidth - LabelWidth) * 0.5f;
    const float LineHeight = ImGui::GetTextLineHeight();
    const float SliderHeight = 28.0f;
    const float Gap = 20.0f;

    const float TotalHeight = (SliderHeight + Gap) * 3.0f - Gap;
    float Y = Layout.ContentTop + (Layout.ContentHeight - TotalHeight) * 0.5f;

    auto DrawSlider = [&](const char *Label, const char *SliderId, float &Value, auto OnChange)
    {
        ImGui::SetCursorPos(ImVec2(StartX, Y + (SliderHeight - LineHeight) * 0.5f));
        ImGui::TextUnformatted(Label);

        ImGui::SetCursorPos(ImVec2(StartX + LabelWidth, Y));
        ImGui::SetNextItemWidth(SliderWidth);
        if (ImGui::SliderFloat(SliderId, &Value, 0.0f, 1.0f, "%.0f%%"))
        {
            OnChange(Value);
        }
        Y += SliderHeight + Gap;
    };

    // 표시용 퍼센트 변환
    float MasterPct = MasterVolume * 100.0f;
    float BgmPct = BgmVolume * 100.0f;
    float SfxPct = SfxVolume * 100.0f;

    ImGui::SetCursorPos(ImVec2(StartX, Y + (SliderHeight - LineHeight) * 0.5f));
    ImGui::TextUnformatted("Master");
    ImGui::SetCursorPos(ImVec2(StartX + LabelWidth, Y));
    ImGui::SetNextItemWidth(SliderWidth);
    if (ImGui::SliderFloat("##Master", &MasterPct, 0.0f, 100.0f, "%.0f%%"))
    {
        MasterVolume = MasterPct / 100.0f;
        FAudioSystem::Get().SetMasterVolume(MasterVolume);
    }
    Y += SliderHeight + Gap;

    ImGui::SetCursorPos(ImVec2(StartX, Y + (SliderHeight - LineHeight) * 0.5f));
    ImGui::TextUnformatted("BGM");
    ImGui::SetCursorPos(ImVec2(StartX + LabelWidth, Y));
    ImGui::SetNextItemWidth(SliderWidth);
    if (ImGui::SliderFloat("##BGM", &BgmPct, 0.0f, 100.0f, "%.0f%%"))
    {
        BgmVolume = BgmPct / 100.0f;
        FAudioSystem::Get().SetChannelVolume(EAudioChannel::BGM, BgmVolume);
    }
    Y += SliderHeight + Gap;

    ImGui::SetCursorPos(ImVec2(StartX, Y + (SliderHeight - LineHeight) * 0.5f));
    ImGui::TextUnformatted("SFX");
    ImGui::SetCursorPos(ImVec2(StartX + LabelWidth, Y));
    ImGui::SetNextItemWidth(SliderWidth);
    if (ImGui::SliderFloat("##SFX", &SfxPct, 0.0f, 100.0f, "%.0f%%"))
    {
        SfxVolume = SfxPct / 100.0f;
        FAudioSystem::Get().SetChannelVolume(EAudioChannel::SFX, SfxVolume);
    }

    ImGui::SetWindowFontScale(1.0f);
}
