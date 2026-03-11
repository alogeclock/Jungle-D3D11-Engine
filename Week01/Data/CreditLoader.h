#pragma once

#include <string>
#include <vector>

struct FCreditEntry
{
    std::string Name;
    std::string Role;
};

class FCreditLoader
{
  public:
    static FCreditLoader &Get();

    bool Initialize(const std::string &Path);
    bool IsLoaded() const;

    bool LoadCredits(std::vector<FCreditEntry> &OutCredits) const;

  private:
    FCreditLoader() = default;

  private:
    std::string FileContent;
    bool        bLoaded = false;
};
