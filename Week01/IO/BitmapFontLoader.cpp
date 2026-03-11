#include "pch.h"
#include "BitmapFontLoader.h"

bool FBitmapFontLoader::LoadFromTextFNT(const std::string& FilePath, FBitmapFont& OutFont)
{
	std::ifstream File(FilePath);
	if (!File.is_open())
	{
		std::cerr << "Failed to open fnt file: " << FilePath << "\n";
		return false;
	}

	OutFont = FBitmapFont{}; //Function output Font

	std::string Line; // Read Each Line
	while (std::getline(File, Line))
	{
		if (Line.empty())
			continue;

		if (StartsWith(Line, "common "))
		{
			ParseCommon(Line, OutFont);
		}
		else if (StartsWith(Line, "page "))
		{
			ParsePage(Line, OutFont);
		}
		else if (StartsWith(Line, "char "))
		{
			FGlyph Glyph;
			if (ParseChar(Line, Glyph))
			{
				OutFont.Glyphs[Glyph.Id] = Glyph;
			}
		}
	}

	return true;
}

/*
* char id=65 x=34 y=50 width=20 height=25 <-- Parse This
* KV =
{
	"lineHeight" → "40"
	"base"       → "26"
	"scaleW"     → "256"
	"scaleH"     → "256"
}
*/
std::unordered_map<std::string, std::string> FBitmapFontLoader::TokenizeKeyValue(const std::string& Line)
{
	std::unordered_map<std::string, std::string> Result;

	std::istringstream ISS(Line);
	std::string Token;

	// 첫 단어(info, common, char...)는 버림
	ISS >> Token;//"id=65"등이 토큰

	while (ISS >> Token) //"id=65"등을 읽고
	{
		size_t EqualPos = Token.find('=');
		if (EqualPos == std::string::npos) //=없는 토큰은 무시
			continue;

		std::string Key = Token.substr(0, EqualPos); // =보다 앞쪽은 Key
		std::string Value = Token.substr(EqualPos + 1);// =보다 뒷쪽은 Value

		// value가 "font atlas.png" 같이 공백 포함한 문자열 일 수 있음
		if (!Value.empty() && Value.front() == '"')
		{
			//back이 따옴표일때까지. 즉, 문자열이 마칠때 까지
			while (!Value.empty() && Value.back() != '"' && ISS.good())
			{
				std::string Next;
				ISS >> Next;
				Value += " " + Next;
			}

			if (Value.size() >= 2 && Value.front() == '"' && Value.back() == '"')
			{
				Value = Value.substr(1, Value.size() - 2);//"abc"에서 a앞의 "랑 c뒤의 "제거
			}
		}

		Result[Key] = Value;
	}

	return Result;
}

int FBitmapFontLoader::ToInt(const std::unordered_map<std::string, std::string>& KV, const std::string& Key, int Default)
{
	auto It = KV.find(Key);
	if (It == KV.end())
		return Default;
	return std::stoi(It->second);
}

std::string FBitmapFontLoader::ToString(const std::unordered_map<std::string, std::string>& KV, const std::string& Key, const std::string& Default)
{
	auto It = KV.find(Key);
	if (It == KV.end())
		return Default;
	return It->second;
}

/*
* common lineHeight=40 base=26 scaleW=256 scaleH=256 <-- Parse Common properties for Glyph
*/
void FBitmapFontLoader::ParseCommon(const std::string& Line, FBitmapFont& OutFont)
{
	auto KV = TokenizeKeyValue(Line);

	OutFont.LineHeight = ToInt(KV, "lineHeight");
	OutFont.Base = ToInt(KV, "base");
	OutFont.ScaleW = ToInt(KV, "scaleW");
	OutFont.ScaleH = ToInt(KV, "scaleH");
	OutFont.ThickNess = ToInt(KV, "thickness");
}

/*
* page id=0 file="font_0.png" <-- Parse File Title
*/
void FBitmapFontLoader::ParsePage(const std::string& Line, FBitmapFont& OutFont)
{
	auto KV = TokenizeKeyValue(Line);
	OutFont.TextureFile = ToString(KV, "file");//FileName
}

bool FBitmapFontLoader::ParseChar(const std::string& Line, FGlyph& OutGlyph)
{
	//
	auto KV = TokenizeKeyValue(Line);

	if (KV.find("id") == KV.end()) //id가 없다면 return false
		return false;

	OutGlyph.Id = ToInt(KV, "id");
	OutGlyph.X = ToInt(KV, "x");
	OutGlyph.Y = ToInt(KV, "y");
	OutGlyph.Width = ToInt(KV, "width");
	OutGlyph.Height = ToInt(KV, "height");
	OutGlyph.XOffset = ToInt(KV, "xoffset");
	OutGlyph.YOffset = ToInt(KV, "yoffset");
	OutGlyph.XAdvance = ToInt(KV, "xadvance");
	OutGlyph.Page = ToInt(KV, "page");
	OutGlyph.Channel = ToInt(KV, "chnl");

	return true;
}
