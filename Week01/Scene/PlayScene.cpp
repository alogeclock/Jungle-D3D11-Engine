#include "PlayScene.h"
#include "../Core/GameContext.h"

void FPlayScene::Enter()
{
}

void FPlayScene::Exit()
{
}

void FPlayScene::Update(FGameContext& Context)
{
}

void FPlayScene::Render(FGameContext& Context)
{
}

void FPlayScene::StartNewGame(const std::string& MapPath)
{
	CurrentMapPath = MapPath;
}

void FPlayScene::RestartGame()
{
}
