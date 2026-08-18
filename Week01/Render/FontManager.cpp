#include "pch.h"
#include "FontManager.h"
#include "IO/BitmapFontLoader.h"
#include "IO/ImageLoader.h"
bool FFontManager::Register(const std::string& Key,
	const std::string& path, std::unique_ptr<FTexture> Texture)
{

	if (Key.empty() || Texture == nullptr)
	{
		return false;
	}
	if (Has(Key))
	{
		return false;
	}
	auto Font = std::make_unique<FBitmapFont>();
	if (!FBitmapFontLoader::LoadFromTextFNT(path, *Font))
	{
		std::cout << "Font Load Failed!";
		return false;
	}
	auto Pair = std::make_unique<FontTexPair>();
	Pair->Font = std::move(Font);
	Pair->Tex = std::move(Texture);
	Fonts.emplace(Key, std::move(Pair));
	return true;
}

FontTexPair* FFontManager::Get(const std::string& Key) const
{
	const auto It = Fonts.find(Key);
	if (It == Fonts.end())
	{
		return nullptr;
	}
	return It->second.get();
}



bool FFontManager::Has(const std::string& Key) const
{
	if (Fonts.find(Key) != Fonts.end())
	{
		return true;
	}
	return false;
}

void FFontManager::Unload(const std::string& Key)
{
	Fonts.erase(Key);
}

void FFontManager::Clear()
{
	Fonts.clear();
}


