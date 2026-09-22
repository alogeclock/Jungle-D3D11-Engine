#pragma once

#include <string>
#include "ThirdParty/nlohmann/json.hpp"

class FJsonFile
{
  public:
    bool LoadFromFile(const std::string &Path);
    bool SaveToFile(const std::string &Path) const;

    nlohmann::json       &GetRoot();
    const nlohmann::json &GetRoot() const;

  private:
    nlohmann::json Root;
};
