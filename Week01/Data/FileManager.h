#pragma once

#include <string>

class FFileManager
{
public:
	static bool Exists(const std::string& Path);
	static std::string ReadAllText(const std::string& Path);
	static bool WriteAllText(const std::string& Path, const std::string& Text);
};
