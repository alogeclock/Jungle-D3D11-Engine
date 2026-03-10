#pragma once
#include <d3d11.h>
class FTexture
{
public:
	int GetWidth() const;
	int GetHeight() const;
	ID3D11Texture2D* Texture2D = nullptr;
	ID3D11ShaderResourceView* TextureSRV = nullptr;

private:
	int Width = 0;
	int Height = 0;
};
