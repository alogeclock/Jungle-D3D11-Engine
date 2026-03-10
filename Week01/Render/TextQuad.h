#pragma once
#include <vector>
#include "Render/BitmapFont.h"
#include "Core/Types.h"
struct FTextQuad
{
	float ScreenX = 0.0f;
	float ScreenY = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;

	float U0 = 0.0f;
	float V0 = 0.0f;
	float U1 = 0.0f;
	float V1 = 0.0f;
};