#pragma once
#include "Core/GameContext.h"
#include "Render/Texture.h"
struct BeatEffect
{
	BeatEffect() {}
	BeatEffect(FTexture* InTexture, float StartX, float StartY) :
		Texture(InTexture), X(StartX), Y(StartY) {
	}
	void Update(float DeltaTime);
	bool IsAlive() const;

	void Render(FGameContext& Context);



	const FTexture* Texture;
	float X = 0.f;
	float Y = 0.f;
private:
	float LifeTime = 0.3f;
	float Speed = 100.f;
};