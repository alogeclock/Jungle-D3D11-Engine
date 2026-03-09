#include "FileManager.h"
#include <fstream>
#include <sstream>

bool FFileManager::Exists(const std::string& Path)
{
	std::ifstream File(Path);
	return File.good();
}

std::string FFileManager::ReadAllText(const std::string& Path)
{
	std::ifstream File(Path);
	if (!File.is_open())
	{
		return "";
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	return Buffer.str();
}

bool FFileManager::WriteAllText(const std::string& Path, const std::string& Text)
{
	std::ofstream File(Path);
	if (!File.is_open())
	{
		return false;
	}

	File << Text;
	return File.good();
}
