#pragma once
#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <iostream>
#include <algorithm>

#include "Render/BitmapFont.h"
class FBitmapFontLoader
{
public:
	static bool LoadFromTextFNT(const std::string& FilePath, FBitmapFont& OutFont);

private:
	static bool StartsWith(const std::string& Str, const std::string& Prefix)
	{
		return Str.rfind(Prefix, 0) == 0;
	}

	static std::unordered_map<std::string, std::string> TokenizeKeyValue(const std::string& Line);

	static int ToInt(const std::unordered_map<std::string, std::string>& KV, const std::string& Key, int Default = 0);

	static std::string ToString(const std::unordered_map<std::string, std::string>& KV, const std::string& Key, const std::string& Default = "");

	static void ParseCommon(const std::string& Line, FBitmapFont& OutFont);

	static void ParsePage(const std::string& Line, FBitmapFont& OutFont);

	static bool ParseChar(const std::string& Line, FGlyph& OutGlyph);
};