#pragma once
#include <string>
#include <unordered_map>
#include "Render/Glyph.h"
struct FBitmapFont
{
	std::string TextureFile; // Font File Name ex) font_0.png
	int LineHeight = 0; // Height when doing Line Change
	int Base = 0; //?
	int ScaleW = 0; // BitmapFont 이미지의 너비
	int ScaleH = 0; // BitmapFont 이미지의 높이

	std::unordered_map<int, FGlyph> Glyphs;

	const FGlyph* Get(int CharCode) const
	{
		auto It = Glyphs.find(CharCode);
		if (It == Glyphs.end())
			return nullptr;
		return &It->second;
	}
};