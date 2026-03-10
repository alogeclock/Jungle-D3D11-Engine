#pragma once
#include <vector>
#include <string>
#include "Core/Types.h"
#include "Texture.h"
struct FRenderObject
{
	FVec2 Position;
	FVec2 Size;
	const FTexture* Texture;
};
