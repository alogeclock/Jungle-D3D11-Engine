#include "pch.h"
#include "Texture.h"
#include "TextureManager.h"

bool FTextureManager::Register(const std::string &Key, std::unique_ptr<FTexture> Texture)
{
    if (Key.empty() || Texture == nullptr)
    {
        return false;
    }

    if (Has(Key))
    {
        return false;
    }

    Textures.emplace(Key, std::move(Texture));
    return true;
}

FTexture *FTextureManager::Get(const std::string &Key) const
{
    const auto It = Textures.find(Key);
    if (It == Textures.end())
    {
        return nullptr;
    }

    return It->second.get();
}

bool FTextureManager::Has(const std::string &Key) const
{
    return Textures.find(Key) != Textures.end();
}

void FTextureManager::Unload(const std::string &Key) { Textures.erase(Key); }

void FTextureManager::Clear() { Textures.clear(); }
