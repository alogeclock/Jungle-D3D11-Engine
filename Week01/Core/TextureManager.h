#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <d3d11.h>
#pragma comment(lib,"d3d11.lib")
class FTexture;

class FTextureManager
{
public:
	bool Load(const std::string& Key, const std::string& Path, ID3D11Device* Device);
	FTexture* Get(const std::string& Key) const;
	bool Has(const std::string& Key) const;

	void Unload(const std::string& Key);
	void Clear();

private:
	std::unordered_map<std::string, std::unique_ptr<FTexture>> Textures;
};
