#include "pch.h"
#include <fstream>
#include <sstream>
#include "FileIO.h"

bool FFileIO::Exists(const std::string &Path)
{
    std::ifstream File(Path);
    return File.good();
}

std::string FFileIO::ReadAllText(const std::string &Path)
{
    std::ifstream File(Path);
    if (!File.is_open())
        return "";

    std::stringstream Buffer;
    Buffer << File.rdbuf();
    return Buffer.str();
}

bool FFileIO::WriteAllText(const std::string &Path, const std::string &Text)
{
    std::ofstream File(Path);
    if (!File.is_open())
        return false;

    File << Text;
    return File.good();
}

std::vector<std::uint8_t> FFileIO::ReadAllBytes(const std::string &Path)
{
    std::ifstream File(Path, std::ios::binary);
    if (!File.is_open())
        return {};

    // 파일 크기 얻기
    File.seekg(0, std::ios::end);
    const std::streamsize Size = File.tellg();
    if (Size <= 0)
        return {};

    // 시작 위치로 되돌림
    File.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> Bytes(static_cast<std::size_t>(Size));
    if (!File.read(reinterpret_cast<char *>(Bytes.data()), Size))
        return {};

    return Bytes;
}
