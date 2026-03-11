#include "pch.h"
#include <fstream>
#include <sstream>
#include "CreditLoader.h"
#include "ThirdParty/nlohmann/json.hpp"

using json = nlohmann::json;

FCreditLoader &FCreditLoader::Get()
{
    static FCreditLoader Instance;
    return Instance;
}

bool FCreditLoader::Initialize(const std::string &Path)
{
    if (bLoaded)
    {
        return true;
    }

    std::ifstream File(Path);
    if (!File.is_open())
    {
        return false;
    }

    std::stringstream Buffer;
    Buffer << File.rdbuf();

    FileContent = Buffer.str();
    bLoaded = true;
    return true;
}

bool FCreditLoader::IsLoaded() const
{
    return bLoaded;
}

bool FCreditLoader::LoadCredits(std::vector<FCreditEntry> &OutCredits) const
{
    if (!bLoaded)
    {
        return false;
    }

    try
    {
        const json Doc = json::parse(FileContent);

        if (!Doc.contains("credits") || !Doc["credits"].is_array())
        {
            return false;
        }

        const json &Credits = Doc["credits"];

        OutCredits.clear();
        OutCredits.reserve(Credits.size());

        for (const auto &EntryJson : Credits)
        {
            if (!EntryJson.is_object())
            {
                return false;
            }

            FCreditEntry Entry;
            Entry.Name = EntryJson.value("name", std::string{});
            Entry.Role = EntryJson.value("role", std::string{});

            OutCredits.push_back(Entry);
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}