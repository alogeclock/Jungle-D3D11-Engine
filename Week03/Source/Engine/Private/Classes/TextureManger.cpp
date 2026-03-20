#include "Source/Engine/Public/Classes/TextureManger.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include <fstream>
#include <iostream>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

class FName;

UTextureManger::UTextureManger(const FString& InString) : UObject(InString) {
	wchar_t ProgramPath[MAX_PATH];
	GetModuleFileNameW(nullptr, ProgramPath, MAX_PATH);

	// Add Root Path
	RootPath = path(ProgramPath).parent_path();
}
UTextureManger::~UTextureManger()
{
	if (!TextureMap.empty()) {
		TextureMap.clear();
	}
}

void UTextureManger::Initialize(URenderer& Renderer)
{
	LoadToFileTexture(RootPath / L"Data", Renderer);
}


FString UTextureManger::GetHashKeyPath(const path& inFilePath) const
{
	path InputPath = inFilePath;      // 사용자 입력 경로
	path AbsolutePath;               // 절대 경로
	path RelativeKeyPath;            // 캐시 키용 경로

	if (InputPath.is_relative())
		AbsolutePath = RootPath / InputPath;
	else
		AbsolutePath = InputPath;

	try
	{
		path CanonicalPath = canonical(AbsolutePath);

		RelativeKeyPath = relative(CanonicalPath, RootPath);
	}
	catch (const filesystem_error&)
	{
		RelativeKeyPath = InputPath;
	}

	return RelativeKeyPath.string();
}

void UTextureManger::LoadToFileTexture(const path& InDirectoryPath, URenderer& Renderer)
{
	ID3D11Device* Device = Renderer.Device;
	ID3D11DeviceContext* DeviceContext = Renderer.DeviceContext;

	if (!Device || !DeviceContext)
	{
		std::cout << "TextureManager: Texture 생성 실패 - Device 또는 DeviceContext가 null입니다" << std::endl;
		return;
	}

	HRESULT ResultHandle;

	std::cout << "TextureManager: 텍스처 로드 시작 - " << InDirectoryPath.string() << std::endl;

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(InDirectoryPath)) {

		if(!Entry.is_regular_file()) continue;

		const path& FilePath = Entry.path();
		FString FileExtension = FilePath.extension().string();

		ComPtr<ID3D11ShaderResourceView> TextureSRV = nullptr;
		try
		{
			if (FileExtension == ".dds") {
				ResultHandle = DirectX::CreateDDSTextureFromFile(Device, DeviceContext,
					FilePath.c_str(), nullptr, TextureSRV.GetAddressOf());
			}
			else if(FileExtension == ".png") {
				ResultHandle = DirectX::CreateWICTextureFromFile(Device, DeviceContext,
					FilePath.c_str(), nullptr, TextureSRV.GetAddressOf());
			}
		}
		catch (const std::exception&)
		{
			ResultHandle = E_FAIL;
		}

		if (SUCCEEDED(ResultHandle) && TextureSRV)
		{
			std::cout << "TextureManager: 텍스처 로드 성공 "
				<< FilePath.string() << std::endl;
		}
		else
		{
			std::cout << "TextureManager: 텍스처 로드 실패 → fallback 사용 "
				<< FilePath.string() << std::endl;

			TextureSRV = GetDefaultTexture(Device);
		}
		FString HashKey = GetHashKeyPath(FilePath);
		TextureMap[HashKey] = TextureSRV;
	}
}

ID3D11ShaderResourceView* UTextureManger::GetTexture(const path& inFilePath)
{
	FString HashKey = GetHashKeyPath(inFilePath);

	if (TextureMap.find(HashKey) != TextureMap.end()) {
		return TextureMap[HashKey].Get();
	}
	
	return nullptr;
}

ComPtr<ID3D11ShaderResourceView> UTextureManger::GetDefaultTexture(ID3D11Device* Device)
{
	static ComPtr<ID3D11ShaderResourceView> DefaultSRV;

	if (DefaultSRV) return DefaultSRV;

	const UINT width = 2;
	const UINT height = 2;

	// 핑크/검정 체크 패턴
	uint32_t pixels[width * height] =
	{
		0xFFFF00FF, 0xFF000000,
		0xFF000000, 0xFFFF00FF
	};

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = pixels;
	initData.SysMemPitch = width * sizeof(uint32_t);

	ComPtr<ID3D11Texture2D> texture;
	Device->CreateTexture2D(&desc, &initData, texture.GetAddressOf());

	Device->CreateShaderResourceView(texture.Get(), nullptr, DefaultSRV.GetAddressOf());

	return DefaultSRV;
}
