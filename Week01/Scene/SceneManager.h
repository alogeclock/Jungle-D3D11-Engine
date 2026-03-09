#pragma once

#include <memory>
#include "../Core/Types.h"

struct FGameContext;
class IScene;

class FSceneManager
{
public:
	void Initialize(FGameContext* InGameContext);

	void RequestChangeScene(ESceneType SceneType);
	void ChangeSceneImmediately(ESceneType SceneType);

	void Update();
	void Render();

	ESceneType GetCurrentSceneType() const;
	IScene* GetCurrentScene();
	const IScene* GetCurrentScene() const;

private:
	void ApplyPendingSceneChange();
	std::unique_ptr<IScene> CreateScene(ESceneType SceneType);

private:
	FGameContext* GameContext = nullptr;

	std::unique_ptr<IScene> CurrentScene;

	ESceneType CurrentSceneType = ESceneType::Title;
	ESceneType PendingSceneType = ESceneType::Title;

	bool bHasPendingSceneChange = false;
};
