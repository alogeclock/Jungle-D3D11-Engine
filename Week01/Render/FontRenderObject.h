#pragma once
#include "Core/Types.h"
#include "Texture.h"
struct FFontRenderObject
{
	FVec2 Offset;
	FVec2 Size;
	FVec2 UV0;
	FVec2 UV1;
	const FTexture* Texture;
};