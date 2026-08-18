#pragma once

#include <string>
#include <vector>
#include "Core/Singleton.h"

struct FCreditEntry
{
    std::string Name;
    std::string Role;
};

class FCreditLoader : public TSingleton<FCreditLoader>
{
    friend class TSingleton<FCreditLoader>;

  public:
    bool Initialize(const std::string &Path);
    bool IsLoaded() const;

    bool LoadCredits(std::vector<FCreditEntry> &OutCredits) const;

  private:
    std::string FileContent;
    bool        bLoaded = false;
};
