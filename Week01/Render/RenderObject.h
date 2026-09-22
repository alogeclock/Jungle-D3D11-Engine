#pragma once
#include <DirectXMath.h>
#include "Texture.h"

struct FRenderObject
{
	const FTexture* Texture = nullptr;
	DirectX::XMFLOAT4X4 World = {};
	DirectX::XMFLOAT2 SpriteSize = { 1.0f, 1.0f };
	DirectX::XMFLOAT2 TextureSize = { 1.0f, 1.0f };
	DirectX::XMFLOAT2 SpriteOffset = { 0.0f, 0.0f };
	float IsMirrored = 0.0f;
	bool bScreenSpace = false;
	DirectX::XMFLOAT4 ColorTint = { 0.0f, 0.0f, 0.0f, 0.0f }; // (0,0,0,0) = no tint
};
