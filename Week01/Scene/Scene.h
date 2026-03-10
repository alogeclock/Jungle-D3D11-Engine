#pragma once

#include "Core/Types.h"

struct FGameContext;

class IScene
{
public:
	virtual ~IScene() = default;

	virtual void Enter() = 0;
	virtual void Exit() = 0;
	virtual void Update(FGameContext& Context) = 0;
	virtual void Render(FGameContext& Context) = 0;

	bool HasSceneChangeRequest() const;
	ESceneType GetRequestedScene() const;
	void ClearSceneChangeRequest();

protected:
	void RequestSceneChange(ESceneType SceneType);

private:
	bool bHasSceneChangeRequest = false;
	ESceneType RequestedScene = ESceneType::Title;
};
