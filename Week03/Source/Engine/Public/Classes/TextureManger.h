#pragma once
#include "Source/Engine/Object/Public/Object.h"
#include <d3d11.h>
#include <filesystem>
#include <wrl.h>

class URenderer;

using namespace std::filesystem;
using namespace Microsoft::WRL;

class UTextureManager :
    public UObject
{
    public:
    static UTextureManager &Get()
    {
        static UTextureManager instance("TextureManagerInstance");
        return instance;
    }
    UTextureManager(const FString &InString);
    ~UTextureManager();  

    void Initialize(URenderer& Renderer);

    FName GetHashKeyPath(const path& inFilePath) const;
    void LoadToFileTexture(const path& inFilePath, URenderer& Renderer);
    ID3D11ShaderResourceView* GetTexture(const FName& FilePath);
    const TMap<FName, ComPtr<ID3D11ShaderResourceView>>& GetTextureMap() const { return TextureMap; }

    ComPtr<ID3D11ShaderResourceView> GetDefaultTexture(ID3D11Device* Device);
private:
	path RootPath;
    //TODO : 현재 FString Key를 FName으로 변경해야함
    TMap<FName, ComPtr<ID3D11ShaderResourceView>> TextureMap;
    ComPtr<ID3D11SamplerState> DefaultSampler; // 추후 샘플러 종류가 많아지면 매핑 형태로 캐싱 후 사용
};

