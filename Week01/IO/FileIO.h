#pragma once

#include <cstdint>
#include <string>
#include <vector>

class FFileIO
{
  public:
    static bool                      Exists(const std::string &Path);
    static std::string               ReadAllText(const std::string &Path);
    static bool                      WriteAllText(const std::string &Path, const std::string &Text);
    static std::vector<std::uint8_t> ReadAllBytes(const std::string &Path);
};
