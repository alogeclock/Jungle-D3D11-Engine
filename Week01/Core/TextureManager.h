#pragma once

#include <memory>
#include <string>
#include <unordered_map>

struct ID3D11Device;
struct ID3D11SamplerState;
struct ID3D11DeviceContext;
class FTexture;

class FTextureManager
{
public:
	~FTextureManager();

	// 초기화: D3D11 디바이스 연결 + 기본 샘플러 생성
	void Initialize(ID3D11Device* InDevice);

	// 텍스처 로드/관리
	bool Load(const std::string& Key, const std::string& Path);
	FTexture* Get(const std::string& Key) const;
	bool Has(const std::string& Key) const;
	void Unload(const std::string& Key);
	void Clear();

	// 기본 샘플러 (Point 필터링 — 픽셀아트용)
	ID3D11SamplerState* GetDefaultSampler() const;

	// 셰이더에 텍스처 + 샘플러 바인딩
	void Bind(const std::string& Key, ID3D11DeviceContext* Context, unsigned int Slot = 0) const;
	void Unbind(ID3D11DeviceContext* Context, unsigned int Slot = 0) const;

private:
	ID3D11Device* Device = nullptr;
	ID3D11SamplerState* DefaultSampler = nullptr;

	std::unordered_map<std::string, std::unique_ptr<FTexture>> Textures;

	void CreateDefaultSampler();
};
