#pragma once

#include <memory>
#include <string>
#include <unordered_map>

class FTexture;

class FTextureManager
{
  public:
    bool      Register(const std::string &Key, std::unique_ptr<FTexture> Texture);
    FTexture *Get(const std::string &Key) const;
    bool      Has(const std::string &Key) const;

    void Unload(const std::string &Key);
    void Clear();

  private:
    std::unordered_map<std::string, std::unique_ptr<FTexture>> Textures;
};
