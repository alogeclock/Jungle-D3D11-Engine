#pragma once
#include "Source/Engine/Object/Public/Object.h"
#include <d3d11.h>
#include <filesystem>
#include <wrl.h>

class URenderer;

using namespace std::filesystem;
using namespace Microsoft::WRL;

class UTextureManger :
    public UObject
{
    public:
    static UTextureManger &Get()
    {
        static UTextureManger instance("TextureManagerInstance");
        return instance;
    }
    UTextureManger(const FString &IａnString);
    ~UTextureManger();

    void Initialize(URenderer& Renderer);

    FString GetHashKeyPath(const path& inFilePath) const;
    void LoadToFileTexture(const path& inFilePath, URenderer& Renderer);
    ID3D11ShaderResourceView* GetTexture(const path& FilePath);
    const TMap<FString, ComPtr<ID3D11ShaderResourceView>>& GetTextureMap() const { return TextureMap; }
private:
  
	path RootPath;
    //TODO : 현재 FString Key를 FName으로 변경해야함
    TMap<FString, ComPtr<ID3D11ShaderResourceView>> TextureMap;
    ComPtr<ID3D11SamplerState> DefaultSampler; // 추후 샘플러 종류가 많아지면 매핑 형태로 캐싱 후 사용
};

