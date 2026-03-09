#include "Application.h"
#include "GameContext.h"
#include "Time.h"
#include "Input.h"
#include "Renderer.h"
#include "TextureManager.h"
#include "../Scene/SceneManager.h"

bool FApplication::Initialize(void* WindowHandle, int ScreenWidth, int ScreenHeight)
{
	return true;
}

void FApplication::Run()
{
}

void FApplication::Shutdown()
{
}

void FApplication::RequestQuit()
{
	bIsRunning = false;
}

bool FApplication::IsRunning() const
{
	return bIsRunning;
}

FTime& FApplication::GetTime()
{
	return *Time;
}

FInput& FApplication::GetInput()
{
	return *Input;
}

FRenderer& FApplication::GetRenderer()
{
	return *Renderer;
}

FTextureManager& FApplication::GetTextureManager()
{
	return *TextureManager;
}

FSceneManager& FApplication::GetSceneManager()
{
	return *SceneManager;
}
