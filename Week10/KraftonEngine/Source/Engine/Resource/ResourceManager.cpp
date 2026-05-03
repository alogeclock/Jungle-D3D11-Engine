#include "Resource/ResourceManager.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <d3d11.h>
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "Core/Log.h"
#include "Profiling/MemoryStats.h"
#include "Engine/Texture/Texture2D.h"
#include "Materials/MaterialManager.h"


namespace ResourceKey
{
	constexpr const char* Font     = "Font";
	constexpr const char* Particle = "Particle";
	constexpr const char* Texture  = "Texture";
	constexpr const char* Mesh     = "Mesh";
	constexpr const char* Material = "Material";
	constexpr const char* PathMap  = "Path";
	constexpr const char* Path     = "Path";
	constexpr const char* Columns  = "Columns";
	constexpr const char* Rows     = "Rows";
}

namespace
{
	uint32 SafeJsonUInt(const json::JSON& Value, uint32 Fallback = 0)
	{
		bool bInt = false;
		const long IntValue = Value.ToInt(bInt);
		if (bInt)
		{
			return static_cast<uint32>(IntValue);
		}

		bool bFloat = false;
		const double FloatValue = Value.ToFloat(bFloat);
		if (bFloat)
		{
			return static_cast<uint32>(FloatValue);
		}

		return Fallback;
	}

	float SafeJsonFloat(const json::JSON& Value, float Fallback = 0.0f)
	{
		bool bFloat = false;
		const double FloatValue = Value.ToFloat(bFloat);
		if (bFloat)
		{
			return static_cast<float>(FloatValue);
		}

		bool bInt = false;
		const long IntValue = Value.ToInt(bInt);
		if (bInt)
		{
			return static_cast<float>(IntValue);
		}

		return Fallback;
	}

	FString ToLowerCopy(FString Value)
	{
		for (char& C : Value)
		{
			C = static_cast<char>(::tolower(static_cast<unsigned char>(C)));
		}
		return Value;
	}
}

void FResourceManager::LoadFromFile(const FString& Path, ID3D11Device* InDevice)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	// Font — { "Name": { "Path": "...", "Columns": 16, "Rows": 16 } }
	if (Root.hasKey(ResourceKey::Font))
	{
		JSON FontSection = Root[ResourceKey::Font];
		for (auto& Pair : FontSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FFontResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows    = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV     = nullptr;
			FontResources[Pair.first] = Resource;
		}
	}

	// Particle — { "Name": { "Path": "...", "Columns": 6, "Rows": 6 } }
	if (Root.hasKey(ResourceKey::Particle))
	{
		JSON ParticleSection = Root[ResourceKey::Particle];
		for (auto& Pair : ParticleSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FParticleResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows    = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV     = nullptr;
			ParticleResources[Pair.first] = Resource;
		}
	}

	// Texture — { "Name": { "Path": "..." } }  (Columns/Rows는 항상 1)
	if (Root.hasKey(ResourceKey::Texture))
	{
		JSON TextureSection = Root[ResourceKey::Texture];
		for (auto& Pair : TextureSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FTextureResource Resource;
			Resource.Name    = FName(Pair.first.c_str());
			Resource.Path    = Entry[ResourceKey::Path].ToString();
			Resource.Columns = 1;
			Resource.Rows    = 1;
			Resource.SRV     = nullptr;
			TextureResources[Pair.first] = Resource;
		}
	}

	// Mesh — { "Name": { "Path": "..." } }  (경로 레지스트리 전용)
	if (Root.hasKey(ResourceKey::Mesh))
	{
		JSON MeshSection = Root[ResourceKey::Mesh];
		for (auto& Pair : MeshSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FMeshResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			MeshResources[Pair.first] = Resource;
		}
	}

	// Material — { "Name": { "Path": "..." } }  (경로 레지스트리 + 기본 프리로드)
	if (Root.hasKey(ResourceKey::Material))
	{
		JSON MaterialSection = Root[ResourceKey::Material];
		for (auto& Pair : MaterialSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FMaterialResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			MaterialResources[Pair.first] = Resource;

			FGenericPathResource PathResource;
			PathResource.Name = Resource.Name;
			PathResource.Path = Resource.Path;
			PathResources[Pair.first] = PathResource;
		}
	}

	if (Root.hasKey(ResourceKey::PathMap))
	{
		JSON PathSection = Root[ResourceKey::PathMap];
		for (auto& Pair : PathSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FGenericPathResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			PathResources[Pair.first] = Resource;
		}
	}

	DiscoverBitmapFonts("Asset/Content/Font");

	if (LoadGPUResources(InDevice))
	{
		UE_LOG("Complete Load Resources!");
	}
	else
	{
		UE_LOG("Failed to Load Resources...");
	}

	for (const auto& [Key, Resource] : MaterialResources)
	{
		FMaterialManager::Get().GetOrCreateMaterial(Resource.Path);
	}
}

void FResourceManager::DiscoverBitmapFonts(const FString& DirectoryPath)
{
	const std::filesystem::path RootPath(FPaths::RootDir());
	const std::filesystem::path FontRoot = RootPath / FPaths::ToWide(DirectoryPath);
	if (!std::filesystem::exists(FontRoot))
	{
		return;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(FontRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		FString Extension = Entry.path().extension().string();
		for (char& C : Extension)
		{
			C = static_cast<char>(::tolower(static_cast<unsigned char>(C)));
		}
		if (Extension != ".json")
		{
			continue;
		}

		FFontResource Resource;
		if (!LoadBitmapFontMetadata(FPaths::ToUtf8(Entry.path().lexically_normal().wstring()), Resource))
		{
			continue;
		}

		const FString FontKey = Resource.Name.ToString();
		if (FontKey.empty() || FontResources.find(FontKey) != FontResources.end())
		{
			continue;
		}

		FontResources[FontKey] = std::move(Resource);
	}
}

bool FResourceManager::LoadBitmapFontMetadata(const FString& JsonPath, FFontResource& OutResource) const
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(JsonPath)));
	if (!File.is_open())
	{
		return false;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);
	if (!Root.hasKey("common") || !Root.hasKey("pages") || !Root.hasKey("chars"))
	{
		return false;
	}

	JSON Common = Root["common"];
	JSON Pages = Root["pages"];
	bool bHasPage = false;
	FString PageFile;
	for (auto& PageValue : Pages.ArrayRange())
	{
		PageFile = PageValue.ToString();
		bHasPage = true;
		break;
	}
	if (!bHasPage)
	{
		return false;
	}

	FString FontName = std::filesystem::path(JsonPath).stem().string();
	if (Root.hasKey("info") && Root["info"].hasKey("face"))
	{
		FontName = Root["info"]["face"].ToString();
	}

	const std::filesystem::path JsonFilePath(FPaths::ToWide(JsonPath));
	const std::filesystem::path ImagePath = JsonFilePath.parent_path() / FPaths::ToWide(PageFile);
	if (!std::filesystem::exists(ImagePath))
	{
		return false;
	}

	const std::filesystem::path RootPath(FPaths::RootDir());
	const std::filesystem::path RelativeImagePath = ImagePath.lexically_normal().lexically_relative(RootPath);

	OutResource = {};
	OutResource.Name = FName(FontName);
	OutResource.Path = FPaths::ToUtf8(RelativeImagePath.generic_wstring());
	OutResource.Columns = 1;
	OutResource.Rows = 1;
	OutResource.AtlasWidth = SafeJsonUInt(Common["scaleW"]);
	OutResource.AtlasHeight = SafeJsonUInt(Common["scaleH"]);
	OutResource.LineHeight = SafeJsonFloat(Common["lineHeight"], 1.0f);
	OutResource.Base = SafeJsonFloat(Common["base"], OutResource.LineHeight);
	OutResource.bHasGlyphMetrics = OutResource.AtlasWidth > 0 && OutResource.AtlasHeight > 0;

	if (!OutResource.bHasGlyphMetrics)
	{
		return false;
	}

	for (auto& CharValue : Root["chars"].ArrayRange())
	{
		const uint32 Codepoint = SafeJsonUInt(CharValue["id"]);
		FFontGlyph Glyph;
		const float X = SafeJsonFloat(CharValue["x"]);
		const float Y = SafeJsonFloat(CharValue["y"]);
		Glyph.Width = SafeJsonFloat(CharValue["width"]);
		Glyph.Height = SafeJsonFloat(CharValue["height"]);
		Glyph.XOffset = SafeJsonFloat(CharValue["xoffset"]);
		Glyph.YOffset = SafeJsonFloat(CharValue["yoffset"]);
		Glyph.XAdvance = SafeJsonFloat(CharValue["xadvance"]);
		Glyph.U0 = X / static_cast<float>(OutResource.AtlasWidth);
		Glyph.V0 = Y / static_cast<float>(OutResource.AtlasHeight);
		Glyph.U1 = (X + Glyph.Width) / static_cast<float>(OutResource.AtlasWidth);
		Glyph.V1 = (Y + Glyph.Height) / static_cast<float>(OutResource.AtlasHeight);
		OutResource.Glyphs[Codepoint] = Glyph;
	}

	if (Root.hasKey("kernings"))
	{
		for (auto& KerningValue : Root["kernings"].ArrayRange())
		{
			const uint32 First = SafeJsonUInt(KerningValue["first"]);
			const uint32 Second = SafeJsonUInt(KerningValue["second"]);
			const uint64 Key = (static_cast<uint64>(First) << 32) | static_cast<uint64>(Second);
			OutResource.Kernings[Key] = SafeJsonFloat(KerningValue["amount"]);
		}
	}

	return !OutResource.Glyphs.empty();
}

void FResourceManager::LoadFromDirectory(const FString& Path, ID3D11Device* InDevice)
{
	const std::filesystem::path RootPath(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(FPaths::ToWide(Path)))
	{
		FString Extension = Entry.path().extension().string();
		for (char& C : Extension)
		{
			C = static_cast<char>(::tolower(static_cast<unsigned char>(C)));
		}
		if (Extension != ".png")
			continue;

		const FString RelativePath = FPaths::ToUtf8(Entry.path().lexically_normal().lexically_relative(RootPath).generic_wstring());
		if (UTexture2D* Texture = UTexture2D::LoadFromFile(RelativePath, InDevice))
		{
			LoadedResource[RelativePath] = Texture->GetSRV();
		}
	}
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	auto LoadSRV = [&](FTextureAtlasResource& Resource) -> bool
	{
		if (Resource.SRV)
		{
			if (Resource.TrackedMemoryBytes > 0)
			{
				MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
				Resource.TrackedMemoryBytes = 0;
			}
			Resource.SRV->Release();
			Resource.SRV = nullptr;
		}

		std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Resource.Path));

		// 확장자에 따라 DDS / WIC 로더 분기
		std::filesystem::path Ext = std::filesystem::path(Resource.Path).extension();
		FString ExtStr = Ext.string();
		for (char& c : ExtStr) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

		HRESULT hr;
		if (ExtStr == ".dds")
		{
			hr = DirectX::CreateDDSTextureFromFileEx(
				Device,
				FullPath.c_str(),
				0,
				D3D11_USAGE_IMMUTABLE,
				D3D11_BIND_SHADER_RESOURCE,
				0, 0,
				DirectX::DDS_LOADER_DEFAULT,
				nullptr,
				&Resource.SRV
			);
		}
		else
		{
			// .png/.jpg/.bmp/.tga 등 — WIC 경유
			hr = DirectX::CreateWICTextureFromFileEx(
				Device,
				FullPath.c_str(),
				0,
				D3D11_USAGE_IMMUTABLE,
				D3D11_BIND_SHADER_RESOURCE,
				0, 0,
				// 이 버전의 DirectXTK 에는 PREMULTIPLY_ALPHA 플래그가 없다.
				// straight-alpha 보간으로 생기는 검은 헤일로는 PS 측 알파 컷오프(0.5) 로 회피.
				DirectX::WIC_LOADER_FORCE_RGBA32,
				nullptr,
				&Resource.SRV
			);
		}
		if (FAILED(hr) || !Resource.SRV)
		{
			return false;
		}

		ID3D11Resource* TextureResource = nullptr;
		Resource.SRV->GetResource(&TextureResource);
		Resource.TrackedMemoryBytes = MemoryStats::CalculateTextureMemory(TextureResource);
		if (TextureResource)
		{
			TextureResource->Release();
		}

		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::AddTextureMemory(Resource.TrackedMemoryBytes);
		}

		return true;
	};

	for (auto& [Key, Resource] : FontResources)
	{
		if (!LoadSRV(Resource)) return false;
	}

	for (auto& [Key, Resource] : ParticleResources)
	{
		if (!LoadSRV(Resource)) return false;
	}

	for (auto& [Key, Resource] : TextureResources)
	{
		if (!LoadSRV(Resource)) return false;
	}

	return true;
}

void FResourceManager::ReleaseGPUResources()
{
	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
			Resource.TrackedMemoryBytes = 0;
		}
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
	for (auto& [Key, Resource] : ParticleResources)
	{
		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
			Resource.TrackedMemoryBytes = 0;
		}
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
	for (auto& [Key, Resource] : TextureResources)
	{
		if (Resource.TrackedMemoryBytes > 0)
		{
			MemoryStats::SubTextureMemory(Resource.TrackedMemoryBytes);
			Resource.TrackedMemoryBytes = 0;
		}
		if (Resource.SRV) { Resource.SRV->Release(); Resource.SRV = nullptr; }
	}
	LoadedResource.clear();
}

// --- Font ---
FFontResource* FResourceManager::FindFont(const FName& FontName)
{
	const FString LookupKey = FontName.ToString();
	auto It = FontResources.find(LookupKey);
	if (It != FontResources.end())
	{
		return &It->second;
	}

	const FString LowerLookupKey = ToLowerCopy(LookupKey);
	for (auto& [Key, Resource] : FontResources)
	{
		if (ToLowerCopy(Key) == LowerLookupKey)
		{
			return &Resource;
		}
	}

	return nullptr;
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	const FString LookupKey = FontName.ToString();
	auto It = FontResources.find(LookupKey);
	return (It != FontResources.end()) ? &It->second : nullptr;

	const FString LowerLookupKey = ToLowerCopy(LookupKey);
	for (const auto& [Key, Resource] : FontResources)
	{
		if (ToLowerCopy(Key) == LowerLookupKey)
		{
			return &Resource;
		}
	}

	return nullptr;
}

void FResourceManager::RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FFontResource Resource;
	Resource.Name    = FontName;
	Resource.Path    = InPath;
	Resource.Columns = Columns;
	Resource.Rows    = Rows;
	Resource.SRV     = nullptr;
	FontResources[FontName.ToString()] = Resource;
}

// --- Particle ---
FParticleResource* FResourceManager::FindParticle(const FName& ParticleName)
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

const FParticleResource* FResourceManager::FindParticle(const FName& ParticleName) const
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FParticleResource Resource;
	Resource.Name    = ParticleName;
	Resource.Path    = InPath;
	Resource.Columns = Columns;
	Resource.Rows    = Rows;
	Resource.SRV     = nullptr;
	ParticleResources[ParticleName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetFontNames() const
{
	TArray<FString> Names;
	Names.reserve(FontResources.size());
	for (const auto& [Key, _] : FontResources)
	{
		Names.push_back(Key);
	}
	std::sort(Names.begin(), Names.end());
	return Names;
}

TArray<FString> FResourceManager::GetParticleNames() const
{
	TArray<FString> Names;
	Names.reserve(ParticleResources.size());
	for (const auto& [Key, _] : ParticleResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

// --- Texture ---
FTextureResource* FResourceManager::FindTexture(const FName& TextureName)
{
	auto It = TextureResources.find(TextureName.ToString());
	return (It != TextureResources.end()) ? &It->second : nullptr;
}

const FTextureResource* FResourceManager::FindTexture(const FName& TextureName) const
{
	auto It = TextureResources.find(TextureName.ToString());
	return (It != TextureResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterTexture(const FName& TextureName, const FString& InPath)
{
	FTextureResource Resource;
	Resource.Name    = TextureName;
	Resource.Path    = InPath;
	Resource.Columns = 1;
	Resource.Rows    = 1;
	Resource.SRV     = nullptr;
	TextureResources[TextureName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetTextureNames() const
{
	TArray<FString> Names;
	Names.reserve(TextureResources.size());
	for (const auto& [Key, _] : TextureResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

FMeshResource* FResourceManager::FindMesh(const FName& MeshName)
{
	auto It = MeshResources.find(MeshName.ToString());
	return (It != MeshResources.end()) ? &It->second : nullptr;
}

const FMeshResource* FResourceManager::FindMesh(const FName& MeshName) const
{
	auto It = MeshResources.find(MeshName.ToString());
	return (It != MeshResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterMesh(const FName& MeshName, const FString& InPath)
{
	FMeshResource Resource;
	Resource.Name = MeshName;
	Resource.Path = InPath;
	MeshResources[MeshName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetMeshNames() const
{
	TArray<FString> Names;
	Names.reserve(MeshResources.size());
	for (const auto& [Key, _] : MeshResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

FMaterialResource* FResourceManager::FindMaterial(const FName& MaterialName)
{
	auto It = MaterialResources.find(MaterialName.ToString());
	return (It != MaterialResources.end()) ? &It->second : nullptr;
}

const FMaterialResource* FResourceManager::FindMaterial(const FName& MaterialName) const
{
	auto It = MaterialResources.find(MaterialName.ToString());
	return (It != MaterialResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterMaterial(const FName& MaterialName, const FString& InPath)
{
	FMaterialResource Resource;
	Resource.Name = MaterialName;
	Resource.Path = InPath;
	MaterialResources[MaterialName.ToString()] = Resource;

	RegisterPath(MaterialName, InPath);
}

TArray<FString> FResourceManager::GetMaterialNames() const
{
	TArray<FString> Names;
	Names.reserve(MaterialResources.size());
	for (const auto& [Key, _] : MaterialResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

FGenericPathResource* FResourceManager::FindPath(const FName& ResourceName)
{
	auto It = PathResources.find(ResourceName.ToString());
	return (It != PathResources.end()) ? &It->second : nullptr;
}

const FGenericPathResource* FResourceManager::FindPath(const FName& ResourceName) const
{
	auto It = PathResources.find(ResourceName.ToString());
	return (It != PathResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterPath(const FName& ResourceName, const FString& InPath)
{
	FGenericPathResource Resource;
	Resource.Name = ResourceName;
	Resource.Path = InPath;
	PathResources[ResourceName.ToString()] = Resource;
}

FString FResourceManager::ResolvePath(const FName& ResourceName, const FString& Fallback) const
{
	const FGenericPathResource* Resource = FindPath(ResourceName);
	return Resource ? Resource->Path : Fallback;
}

TArray<FString> FResourceManager::GetPathNames() const
{
	TArray<FString> Names;
	Names.reserve(PathResources.size());
	for (const auto& [Key, _] : PathResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FResourceManager::FindLoadedTexture(FString InPath)
{
	auto It = LoadedResource.find(InPath);
	return (It != LoadedResource.end()) ? It->second : nullptr;
}

