#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "Render/Texture.h"
#include "Render/BitmapFont.h"
#include "Render/TextQuad.h"

struct FontTexPair
{
	std::unique_ptr<FBitmapFont> Font;
	std::unique_ptr<FTexture> Tex;
};
class FFontManager
{
public:
	bool      Register(const std::string& Key, const std::string& path, std::unique_ptr<FTexture> Texture);
	FontTexPair* Get(const std::string& Key) const;
	bool      Has(const std::string& Key) const;

	void Unload(const std::string& Key);
	void Clear();

private:
	std::unordered_map<std::string, std::unique_ptr<FontTexPair>> Fonts;
};


static std::vector<FTextQuad> BuildQuads(const FBitmapFont& Font,
	const std::string& Text, float StartX, float StartY, float ScaleFactor = 1.0f)
{
	std::vector<FTextQuad> Quads;
	FVec2 Cursor;
	Cursor.X = StartX; Cursor.Y = StartY;
	for (char c : Text)
	{
		const FGlyph* Glyph = Font.Get(c);
		if (!Glyph)
		{
			continue;
		}

		FTextQuad Quad;
		Quad.Width = Glyph->Width * ScaleFactor;
		Quad.Height = Glyph->Height * ScaleFactor;

		// BMFont offsets are relative to top-left of cursor.
		// Since we draw from center (-0.5 to 0.5), we add half-width/height.
		Quad.ScreenX = Cursor.X + Glyph->XOffset * ScaleFactor + (Quad.Width * 0.5f);
		Quad.ScreenY = Cursor.Y + Glyph->YOffset * ScaleFactor + (Quad.Height * 0.5f);

		Quad.U0 = Glyph->X / static_cast<float>(Font.ScaleW);
		Quad.V0 = Glyph->Y / static_cast<float>(Font.ScaleH);
		Quad.U1 = (Glyph->X + Glyph->Width) / static_cast<float>(Font.ScaleW);
		Quad.V1 = (Glyph->Y + Glyph->Height) / static_cast<float>(Font.ScaleH);

		Cursor.X += static_cast<float>(Glyph->XAdvance) * ScaleFactor + Font.ThickNess * ScaleFactor;

		Quads.push_back(Quad);
	}

	return Quads;
}