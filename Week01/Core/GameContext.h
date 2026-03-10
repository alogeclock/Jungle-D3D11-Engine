#pragma once

class FTime;
class FInput;
class FRenderer;
class FTextureManager;
class FFontManager;

struct FGameContext
{
	FTime& Time;
	FInput& Input;
	FRenderer& Renderer;
	FTextureManager& Textures;
	FFontManager& FontManager;
};
