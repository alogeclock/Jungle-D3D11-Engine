#include "TextureManager.h"
#include "Texture.h"

bool FTextureManager::Load(const std::string& Key, const std::string& Path)
{
	return false;
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
