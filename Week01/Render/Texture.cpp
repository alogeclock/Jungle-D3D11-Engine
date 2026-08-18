#include "pch.h"
#include <d3d11.h>
#include <utility>
#include "Texture.h"


FTexture::FTexture(int InWidth, int InHeight, ID3D11Texture2D *InTexture2D,
                   ID3D11ShaderResourceView *InTextureSRV)
    : Width(InWidth), Height(InHeight), Texture2D(InTexture2D), TextureSRV(InTextureSRV)
{
}

FTexture::~FTexture()
{
    if (TextureSRV != nullptr)
    {
        TextureSRV->Release();
        TextureSRV = nullptr;
    }

    if (Texture2D != nullptr)
    {
        Texture2D->Release();
        Texture2D = nullptr;
    }
}

FTexture::FTexture(FTexture &&Other) noexcept
    : Width(Other.Width), Height(Other.Height), Texture2D(Other.Texture2D),
      TextureSRV(Other.TextureSRV)
{
    Other.Width = 0;
    Other.Height = 0;
    Other.Texture2D = nullptr;
    Other.TextureSRV = nullptr;
}

FTexture &FTexture::operator=(FTexture &&Other) noexcept
{
    if (this == &Other)
    {
        return *this;
    }

    if (TextureSRV != nullptr)
    {
        TextureSRV->Release();
    }

    if (Texture2D != nullptr)
    {
        Texture2D->Release();
    }

    Width = Other.Width;
    Height = Other.Height;
    Texture2D = Other.Texture2D;
    TextureSRV = Other.TextureSRV;

    Other.Width = 0;
    Other.Height = 0;
    Other.Texture2D = nullptr;
    Other.TextureSRV = nullptr;

    return *this;
}
