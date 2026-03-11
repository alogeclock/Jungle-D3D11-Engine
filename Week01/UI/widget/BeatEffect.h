#pragma once
#include "Render/Texture.h"
struct BeatEffect
{
	BeatEffect(FTexture* InTexture) : Texture(InTexture) {}
	void Update(float DeltaTime);
	bool IsAlive() const;
	const FTexture* Texture;
private:
	float LifeTime = 0.3f;
};