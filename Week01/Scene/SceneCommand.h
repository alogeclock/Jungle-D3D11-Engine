#pragma once

#include "SceneType.h"

enum class ESceneCommandType
{
    None,
    ChangeScene,
    QuitGame
};

struct FSceneCommand
{
    ESceneCommandType Type = ESceneCommandType::None;
    ESceneType        NextScene = ESceneType::Title;
    int               NextStageIndex = -1;
    int               AccumulatedScore = 0;
};
