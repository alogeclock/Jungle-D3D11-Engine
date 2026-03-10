#include "pch.h"
#include "Scene.h"

bool IScene::HasSceneChangeRequest() const
{
	return bHasSceneChangeRequest;
}

ESceneType IScene::GetRequestedScene() const
{
	return RequestedScene;
}

void IScene::ClearSceneChangeRequest()
{
	bHasSceneChangeRequest = false;
}

void IScene::RequestSceneChange(ESceneType SceneType)
{
	bHasSceneChangeRequest = true;
	RequestedScene = SceneType;
}
