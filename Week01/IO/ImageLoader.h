#pragma once

#include <memory>
#include <string>

struct ID3D11Device;
class FTexture;

// 현재는 png 파일만 지원
class FImageLoader
{
  public:
    static std::unique_ptr<FTexture> LoadAsTexture(ID3D11Device *Device, const std::string &Path);

  private:
    static bool IsPngFile(const std::string &Path);
};
