#pragma once

struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

class FTexture
{
  public:
    FTexture() = default;
    FTexture(int InWidth, int InHeight, ID3D11Texture2D *InTexture2D,
             ID3D11ShaderResourceView *InTextureSRV);
    ~FTexture();

    FTexture(const FTexture &) = delete;
    FTexture &operator=(const FTexture &) = delete;

    FTexture(FTexture &&Other) noexcept;
    FTexture &operator=(FTexture &&Other) noexcept;

    int Width = 0;
    int Height = 0;

    ID3D11Texture2D          *GetTexture2D() const { return Texture2D; }
    ID3D11ShaderResourceView *GetTextureSRV() const { return TextureSRV; }

  private:
    ID3D11Texture2D          *Texture2D = nullptr;
    ID3D11ShaderResourceView *TextureSRV = nullptr;
};
