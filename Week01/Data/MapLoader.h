#pragma once

#include <string>

class FMapData;

class FMapLoader
{
public:
	bool LoadFromFile(const std::string& Path, FMapData& OutMap);
	bool SaveToFile(const std::string& Path, const FMapData& Map);
};
