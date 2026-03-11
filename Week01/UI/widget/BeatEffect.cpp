#include "pch.h"
#include "BeatEffect.h"
#include "Render/Renderer.h"
void BeatEffect::Update(float DeltaTime)
{
	LifeTime -= DeltaTime;
	Y -= Speed * DeltaTime;
}

bool BeatEffect::IsAlive() const
{
	return LifeTime >= 0.f;
}

void BeatEffect::Render(FGameContext& Context)
{
	Context.Renderer.DrawTexture(Texture, X, Y, 200, 100);
}
