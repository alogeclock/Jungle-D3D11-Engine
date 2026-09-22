#pragma once

#include <memory>
#include "Scene.h"
#include "SceneCommand.h"

enum class ESceneManagerUpdateResult
{
    None,
    QuitGame
};

class FSceneManager
{
  public:
    void                      Initialize();
    ESceneManagerUpdateResult Update(FGameContext &Context);
    void                      Render(FGameContext &Context);

  private:
    void HandleSceneCommand(const FSceneCommand &Command);
    void ChangeSceneInternal(ESceneType SceneType, int StageIndex = -1, int AccumulatedScore = 0,
                             const std::string &PlayerSkinKey = "");

  private:
    std::unique_ptr<IScene> CurrentScene;
};
