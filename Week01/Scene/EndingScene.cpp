#include "pch.h"
#include "EndingScene.h"
#include <memory>

#include "Core/GameContext.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include "UI/popup/EndingPopup.h"
#include "UI/popup/GoToTitlePopup.h"
#include "UI/popup/PopupManager.h"
#include "UI/popup/UIPopupAction.h"
#include "Data/ScoreRepository.h"
#include "UI/popup/SaveScorePopup.h"

#define WIN_WIDTH 1024
#define WIN_HEIGHT 1024

void FEndingScene::Update(FGameContext &Context)
{
    if (!bOpenedEndingPopup)
    {
        ElapsedTime += Context.Time.GetDeltaTime();

        if (ElapsedTime >= PopupOpenDelay)
        {
            OpenEndingPopup();
            bOpenedEndingPopup = true;
        }
    }

    UIManager.Update(Context);
}

void FEndingScene::Render(FGameContext &Context)
{
    Context.Renderer.DrawTexture(Context.Textures.Get("ending"), WIN_WIDTH * 0.5f,
                                 WIN_HEIGHT * 0.5f, WIN_WIDTH, WIN_HEIGHT);

    UIManager.Render(Context);

    HandlePopupResult(Context);
    UIManager.GetPopupManager().RemoveClosedPopup();
}

void FEndingScene::HandlePopupResult(FGameContext &Context)
{
    FPopupManager &PopupManager = UIManager.GetPopupManager();

    if (FEndingPopup *Popup = PopupManager.GetPopup<FEndingPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FSaveScorePopup *Popup = PopupManager.GetPopup<FSaveScorePopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FGoToTitlePopup *Popup = PopupManager.GetPopup<FGoToTitlePopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }
}

void FEndingScene::OpenEndingPopup()
{
    std::unique_ptr<FEndingPopup> Popup = std::make_unique<FEndingPopup>();
    Popup->SetMessages(Messages);
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FEndingScene::OpenGoToTitlePopup()
{
    std::unique_ptr<FGoToTitlePopup> Popup = std::make_unique<FGoToTitlePopup>();
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

bool FEndingScene::HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                                        EUIPopupAction Action)
{
    switch (Action)
    {
        case EUIPopupAction::OpenSaveScorePopup:
        {
            Popup.Close();
            OpenSaveScorePopup();
            return true;
        }

        case EUIPopupAction::ConfirmSaveScore:
        {
            Popup.Close();

            FSaveScorePopup *SavePopup = dynamic_cast<FSaveScorePopup *>(&Popup);
            if (!SavePopup)
                return true;

            const std::string Nickname = SavePopup->GetNickname();
            const int ClearedStage = 4; // 실제 값으로 교체
            const int Score = 0;        // 실제 값으로 교체

            ScoreRepository::AppendRecord({Nickname, ClearedStage, Score});
            OpenGoToTitlePopup();
            return true;
        }

        case EUIPopupAction::OpenGoToTitlePopup:
        {
            Popup.Close();
            OpenGoToTitlePopup();
            return true;
        }

        default:
            break;
    }

    return false;
}

void FEndingScene::OpenSaveScorePopup()
{
    std::unique_ptr<FSaveScorePopup> Popup = std::make_unique<FSaveScorePopup>();
    Popup->SetScore(0);   // 실제 최종 점수로 교체
    Popup->SetStage(4);   // 실제 마지막 스테이지 번호로 교체
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}