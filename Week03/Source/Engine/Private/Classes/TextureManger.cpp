#include "Source/Engine/Public/Classes/TextureManger.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include <fstream>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

class FName;

UTextureManager::UTextureManager(const FString& InString) : UObject(InString) {
	wchar_t ProgramPath[MAX_PATH];
	GetModuleFileNameW(nullptr, ProgramPath, MAX_PATH);

	// Add Root Path
	RootPath = path(ProgramPath).parent_path();
}

UTextureManager::~UTextureManager()
{
	if (!TextureMap.empty()) {
		TextureMap.clear();
	}
}

void UTextureManager::Initialize(URenderer& Renderer)
{
	LoadToFileTexture("Data/Texture", Renderer);
}


FName UTextureManager::GetHashKeyPath(const path& inFilePath) const
{
	path InputPath = inFilePath;      // 사용자 입력 경로
	path AbsolutePath;               // 절대 경로
	path RelativeKeyPath;            // 캐시 키용 경로
	
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

void UTextureManager::LoadToFileTexture(const path& InDirectoryPath, URenderer& Renderer)
{
	if (!std::filesystem::exists(InDirectoryPath))
	{
		// 경로상의 모든 상위 디렉토리까지 포함하여 생성
		std::filesystem::create_directories(InDirectoryPath);
	}
	else if (!std::filesystem::is_directory(InDirectoryPath))
	{
		return;
	}

	ID3D11Device* Device = Renderer.Device;
	ID3D11DeviceContext* DeviceContext = Renderer.DeviceContext;

	if (!Device || !DeviceContext)
	{
		return;
	}

	HRESULT ResultHandle;

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(InDirectoryPath)) {

		if(!Entry.is_regular_file()) continue;

		const path& FilePath = Entry.path();
		FString FileExtension = FilePath.extension().string();

		ComPtr<ID3D11ShaderResourceView> TextureSRV = nullptr;

		if (FileExtension == ".dds") {
			ResultHandle = DirectX::CreateDDSTextureFromFile(Device, DeviceContext,
				FilePath.c_str(), nullptr, TextureSRV.GetAddressOf());
		}
		else if(FileExtension == ".png") {
			ResultHandle = DirectX::CreateWICTextureFromFile(Device, DeviceContext,
				FilePath.c_str(), nullptr, TextureSRV.GetAddressOf());
		}

		if (SUCCEEDED(ResultHandle))
		{
			FName HashKey = GetHashKeyPath(FilePath);
			TextureMap[HashKey] = TextureSRV;
		}
	}
}

ID3D11ShaderResourceView* UTextureManager::GetTexture(const FName& inFilePath)
{
	FName HashKey = GetHashKeyPath(inFilePath.ToString());
	
	
    const auto& It = TextureMap.find(HashKey);
    if (It != TextureMap.end())
    {
        return It->second.Get();
    }
	return nullptr;
}
