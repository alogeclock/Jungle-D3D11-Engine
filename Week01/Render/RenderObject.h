#pragma once
#include <vector>
#include <string>
#include "Types.h"
#include "Texture.h"
struct FRenderObject
{
	FVec2 Position;
	FVec2 Size;
	const FTexture* Texture;
};
