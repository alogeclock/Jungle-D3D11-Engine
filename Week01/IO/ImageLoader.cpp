#include <algorithm>
#include <cctype>
#include <cstdint>
#include <d3d11.h>
#include <stdexcept>
#include <vector>
#include "FileIO.h"
#include "ImageLoader.h"
#include "Render/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "ThridParty/stb_image.h"

namespace
{
    std::string ToLower(std::string Text)
    {
        std::transform(Text.begin(), Text.end(), Text.begin(),
                       [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
        return Text;
    }
} // namespace

std::unique_ptr<FTexture> FImageLoader::LoadAsTexture(ID3D11Device *Device, const std::string &Path)
{
    if (Device == nullptr)
    {
        return nullptr;
    }

    // TODO: nullptr 반환으로 예외처리
    if (!IsPngFile(Path))
    {
        throw std::runtime_error("FImageLoader only supports .png files.");
    }

    const std::vector<std::uint8_t> Bytes = FFileIO::ReadAllBytes(Path);
    if (Bytes.empty())
    {
        return nullptr;
    }

    int Width = 0;
    int Height = 0;
    int Channels = 0;

    unsigned char *RawPixels = stbi_load_from_memory(Bytes.data(), static_cast<int>(Bytes.size()),
                                                     &Width, &Height, &Channels, 4);

    if (RawPixels == nullptr)
    {
        return nullptr;
    }

    ID3D11Texture2D          *Texture2D = nullptr;
    ID3D11ShaderResourceView *TextureSRV = nullptr;

    D3D11_TEXTURE2D_DESC TextureDesc{};
    TextureDesc.Width = static_cast<UINT>(Width);
    TextureDesc.Height = static_cast<UINT>(Height);
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA InitialData{};
    InitialData.pSysMem = RawPixels;
    InitialData.SysMemPitch = static_cast<UINT>(Width * 4);
    InitialData.SysMemSlicePitch = 0;

    HRESULT Hr = Device->CreateTexture2D(&TextureDesc, &InitialData, &Texture2D);

    stbi_image_free(RawPixels);
    RawPixels = nullptr;

    if (FAILED(Hr) || Texture2D == nullptr)
    {
        if (Texture2D != nullptr)
        {
            Texture2D->Release();
        }
        return nullptr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
    SRVDesc.Format = TextureDesc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;

    Hr = Device->CreateShaderResourceView(Texture2D, &SRVDesc, &TextureSRV);
    if (FAILED(Hr) || TextureSRV == nullptr)
    {
        Texture2D->Release();
        return nullptr;
    }

    return std::make_unique<FTexture>(Width, Height, Texture2D, TextureSRV);
}

bool FImageLoader::IsPngFile(const std::string &Path)
{
    const std::string LowerPath = ToLower(Path);

    const std::size_t DotIndex = LowerPath.find_last_of('.');
    if (DotIndex == std::string::npos)
    {
        return false;
    }

    const std::string Extension = LowerPath.substr(DotIndex);
    return Extension == ".png";
}
