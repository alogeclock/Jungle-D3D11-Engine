#include "TextureManager.h"
#include "Texture.h"
#include <wrl/client.h>

bool FTextureManager::Load(const std::string& Key, const std::string& Path, ID3D11Device* Device)
{
	if (Textures.find(Key) != Textures.end())
	{
		return true;
	}
	FTexture Texture;
	std::wstring wPath(Path.begin(), Path.end());

	//HRESULT hr = DirectX::CreateWICTextureFromFile(
	//	Device,
	//	wPath.c_str(),
	//	nullptr,
	//	&Texture.TextureSRV
	//);
	return true;
}

FTexture* FTextureManager::Get(const std::string& Key) const
{
	auto It = Textures.find(Key);
	if (It != Textures.end())
	{
		return It->second.get();
	}
	return nullptr;
}

bool FTextureManager::Has(const std::string& Key) const
{
	return Textures.find(Key) != Textures.end();
}

void FTextureManager::Unload(const std::string& Key)
{
	Textures.erase(Key);
}

void FTextureManager::Clear()
{
	Textures.clear();
}
