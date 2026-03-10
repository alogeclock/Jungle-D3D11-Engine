#include "TextureManager.h"
#include "Texture.h"
#include <wrl/client.h>

#include <d3d11.h>
#include <objbase.h>

FTextureManager::~FTextureManager()
{
	Clear();
	if (DefaultSampler) { DefaultSampler->Release(); DefaultSampler = nullptr; }
}

void FTextureManager::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;

	// COM 초기화 (WIC 사용을 위해)
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	CreateDefaultSampler();
}

bool FTextureManager::Load(const std::string& Key, const std::string& Path)
{
	if (!Device) return false;

	// 이미 로드된 텍스처면 스킵
	if (Has(Key)) return true;

	auto Tex = std::make_unique<FTexture>();
	if (!Tex->LoadFromFile(Device, Path))
	{
		return false;
	}

	Textures[Key] = std::move(Tex);
	return true;
}

FTexture* FTextureManager::Get(const std::string& Key) const
{
	auto It = Textures.find(Key);
	if (It != Textures.end())
	{
		return It->second.get();
	}
	return nullptr;
}

bool FTextureManager::Has(const std::string& Key) const
{
	return Textures.find(Key) != Textures.end();
}

void FTextureManager::Unload(const std::string& Key)
{
	Textures.erase(Key);
}

void FTextureManager::Clear()
{
	Textures.clear();
}

ID3D11SamplerState* FTextureManager::GetDefaultSampler() const
{
	return DefaultSampler;
}

void FTextureManager::Bind(const std::string& Key, ID3D11DeviceContext* Context, UINT Slot) const
{
	FTexture* Tex = Get(Key);
	if (Tex && Tex->GetSRV())
	{
		ID3D11ShaderResourceView* SRV = Tex->GetSRV();
		Context->PSSetShaderResources(Slot, 1, &SRV);
	}

	if (DefaultSampler)
	{
		Context->PSSetSamplers(Slot, 1, &DefaultSampler);
	}
}

void FTextureManager::Unbind(ID3D11DeviceContext* Context, UINT Slot) const
{
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Context->PSSetShaderResources(Slot, 1, &NullSRV);
}

void FTextureManager::CreateDefaultSampler()
{
	if (!Device) return;

	D3D11_SAMPLER_DESC Desc = {};
	Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;  // 픽셀아트용 포인트 필터링
	Desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	Desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	Desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	Device->CreateSamplerState(&Desc, &DefaultSampler);
}
