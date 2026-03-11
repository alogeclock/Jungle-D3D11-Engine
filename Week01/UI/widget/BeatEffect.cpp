#include "pch.h"
#include "BeatEffect.h"

void BeatEffect::Update(float DeltaTime)
{
	LifeTime -= DeltaTime;
}

bool BeatEffect::IsAlive() const
{
	return LifeTime >= 0.f;
}
