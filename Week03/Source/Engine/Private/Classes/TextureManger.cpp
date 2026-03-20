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

		if (FileExtension == ".dds") {
			ResultHandle = DirectX::CreateDDSTextureFromFile(Device, DeviceContext,
				FilePath.c_str(), nullptr, TextureSRV.GetAddressOf());
		}
		else {
			ResultHandle = DirectX::CreateWICTextureFromFile(Device, DeviceContext,
				FilePath.c_str(), nullptr, TextureSRV.GetAddressOf());
		}

		if (SUCCEEDED(ResultHandle))
		{
			FString HashKey = GetHashKeyPath(FilePath);
			std::cout << "TextureManager: 텍스처 로드 성공" << FilePath.string() << std::endl;
			TextureMap[HashKey] = TextureSRV;
		}
		else
		{
			std::cout << "TextureManager: 텍스처 로드 실패..." << FilePath.string() << ResultHandle << std::endl;
		}
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
