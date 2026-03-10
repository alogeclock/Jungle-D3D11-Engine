#include "FileIO.h"
#include "JsonFile.h"

bool FJsonFile::LoadFromFile(const std::string &Path)
{
    const std::string Text = FFileIO::ReadAllText(Path);
    if (Text.empty())
    {
        return false;
    }

    try
    {
        Root = nlohmann::json::parse(Text);
    }
    catch (...)
    {
        Root = nlohmann::json{};
        return false;
    }

    return true;
}

bool FJsonFile::SaveToFile(const std::string &Path) const
{
    return FFileIO::WriteAllText(Path, Root.dump(4));
}

nlohmann::json &FJsonFile::GetRoot() { return Root; }

const nlohmann::json &FJsonFile::GetRoot() const { return Root; }
