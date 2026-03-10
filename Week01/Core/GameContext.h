#pragma once

class FTime;
class FInput;
class FRenderer;
class FTextureManager;

struct FGameContext
{
    FTime           &Time;
    FInput          &Input;
    FRenderer       &Renderer;
    FTextureManager &Textures;
};
