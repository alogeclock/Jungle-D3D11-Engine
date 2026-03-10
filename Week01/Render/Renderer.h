#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include "../Gameplay/Camera2D.h"
#include "Texture.h"
#include "Types.h"

class FRenderer
{
  private:
    int ScreenWidth = 0;
    int ScreenHeight = 0;

    void *Device = nullptr;
    void *DeviceContext = nullptr;
    void *SwapChain = nullptr;
    void *RenderTargetView = nullptr;

  public:
    bool Initialize(void *windowHandle, int screenWidth, int screenHeight);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void DrawTexture(const FTexture *texture, float screenX, float screenY, float width,
                     float height);

    void DrawTextureInWorld(const FTexture *texture, float worldX, float worldY, float width,
                            float height, const FCamera2D &camera);

    int GetScreenWidth() const;
    int GetScreenHeight() const;
};