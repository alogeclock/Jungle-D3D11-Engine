#pragma once

#include "SceneCommand.h"

struct FGameContext;

class IScene
{
  public:
    virtual ~IScene() = default;

    virtual void Update(FGameContext &Context) = 0;
    virtual void Render(FGameContext &Context) = 0;

    virtual FSceneCommand ConsumeCommand()
    {
        FSceneCommand Command = PendingCommand;
        PendingCommand = {};
        return Command;
    }

  protected:
    void SetSceneCommand(const FSceneCommand &Command) { PendingCommand = Command; }
    void SetChangeSceneCommand(ESceneType type)
    {
        FSceneCommand Command;
        Command.Type = ESceneCommandType::ChangeScene;
        Command.NextScene = type;
        SetSceneCommand(Command);
    }

  private:
    FSceneCommand PendingCommand;
};
