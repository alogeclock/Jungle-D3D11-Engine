#include "pch.h"
#include "BeatHUDWidget.h"
#include "Core/Time.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include <cmath>

void FBeatHUDWidget::BindBeatSystem(const FBeatSystem* InBeatSystem)
{
	BeatSystem = InBeatSystem;
	Ypos = 800.f;
	Heart.X = 500.f;
	Heart.Y = Ypos;

}

void FBeatHUDWidget::Update(FGameContext& Context)
{
	if (FlashTimer > 0.0f)
	{
		FlashTimer -= Context.Time.GetDeltaTime();
	}
}

void FBeatHUDWidget::Render(FGameContext& Context)
{

	if (!BeatSystem) return;

	float BeatInterval = BeatSystem->GetBeatInterval();
	//이전박자 ----- 현재 ----- 이후박자
	//    ElapsedTime   RemainedTime
	float RemainedTime = BeatSystem->GetTimeToNextBeat(); //거리계산에 사용
	float ElapsedTime = BeatInterval - RemainedTime;
	float DistanceRatio = RemainedTime / BeatInterval;
	float Distance = MaxDistance * DistanceRatio;

	float LeftBarX = Heart.X - Distance;
	float RightBarX = Heart.X + Distance;
	Context.Renderer.DrawTexture(BarTexture, LeftBarX, Ypos, 20, 100);
	Context.Renderer.DrawTexture(BarTexture, RightBarX, Ypos, 20, 100);
	Context.Renderer.DrawTexture(HeartTexture, Heart.X, Heart.Y, 120, 150);
}

void FBeatHUDWidget::SetTextures(FGameContext& Context)
{
	if (!BarTexture)
	{
		BarTexture = Context.Textures.Get("beat_bar");
	}
	if (!HeartTexture)
	{
		HeartTexture = Context.Textures.Get("beat_heart");
	}
}
