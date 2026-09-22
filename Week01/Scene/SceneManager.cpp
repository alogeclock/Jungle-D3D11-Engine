#include "pch.h"
#include "PlayScene.h"
#include "SceneManager.h"
#include "SceneType.h"
#include "TestScene.h"
#include "TitleScene.h"
#include "EndingScene.h"

void FSceneManager::Initialize() { ChangeSceneInternal(ESceneType::Title); }

ESceneManagerUpdateResult FSceneManager::Update(FGameContext &Context)
{
    if (!CurrentScene)
        return ESceneManagerUpdateResult::None;

    CurrentScene->Update(Context);

    const FSceneCommand Command = CurrentScene->ConsumeCommand();

    if (Command.Type == ESceneCommandType::QuitGame)
        return ESceneManagerUpdateResult::QuitGame;

    HandleSceneCommand(Command);
    return ESceneManagerUpdateResult::None;
}

void FSceneManager::Render(FGameContext &Context)
{
    if (!CurrentScene)
        return;
    CurrentScene->Render(Context);
}

void FSceneManager::HandleSceneCommand(const FSceneCommand &Command)
{
    switch (Command.Type)
    {
    case ESceneCommandType::None:
        break;

    case ESceneCommandType::ChangeScene:
        ChangeSceneInternal(Command.NextScene, Command.NextStageIndex, Command.AccumulatedScore,
                            Command.PlayerSkinKey);
        break;

    case ESceneCommandType::QuitGame:
        break;

    default:
        break;
    }
}

void FSceneManager::ChangeSceneInternal(ESceneType SceneType, int StageIndex, int AccumulatedScore,
                                        const std::string &PlayerSkinKey)
{
    switch (SceneType)
    {
    case ESceneType::Test:
        CurrentScene = std::make_unique<FTestScene>();
        break;

    case ESceneType::Title:
        CurrentScene = std::make_unique<FTitleScene>();
        break;

    case ESceneType::Play:
        CurrentScene = std::make_unique<FPlayScene>(StageIndex, AccumulatedScore, PlayerSkinKey);
        break;

    case ESceneType::Ending:
        CurrentScene = std::make_unique<FEndingScene>(AccumulatedScore, PlayerSkinKey);
        break;

    default:
        CurrentScene.reset();
        break;
    }
}
