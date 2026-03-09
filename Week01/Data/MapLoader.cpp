#include "MapLoader.h"
#include "../Gameplay/MapData.h"

#include <fstream>
#include <sstream>

bool FMapLoader::LoadFromFile(const std::string& Path, FMapData& OutMap)
{
	std::ifstream File(Path);
	if (!File.is_open())
	{
		return false;
	}

	int Width = 0;
	int Height = 0;
	File >> Width >> Height;

	if (Width <= 0 || Height <= 0)
	{
		return false;
	}

	OutMap.Resize(Width, Height);

	for (int Y = 0; Y < Height; Y++)
	{
		for (int X = 0; X < Width; X++)
		{
			int Value = 0;
			File >> Value;
			OutMap.SetTile(X, Y, Value);
		}
	}

	return true;
}

bool FMapLoader::SaveToFile(const std::string& Path, const FMapData& Map)
{
	std::ofstream File(Path);
	if (!File.is_open())
	{
		return false;
	}

	File << Map.GetWidth() << " " << Map.GetHeight() << "\n";

	for (int Y = 0; Y < Map.GetHeight(); Y++)
	{
		for (int X = 0; X < Map.GetWidth(); X++)
		{
			if (X > 0) File << " ";
			File << Map.GetTile(X, Y);
		}
		File << "\n";
	}

	return true;
}
