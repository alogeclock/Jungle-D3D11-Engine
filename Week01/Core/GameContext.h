#pragma once

#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include "Core/Time.h"
#include "Core/Input.h"

struct FGameContext
{
    FTime           &Time;
    FInput          &Input;
    FRenderer       &Renderer;
    FTextureManager &Textures;
};
