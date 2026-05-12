#include "Shader.h"
#include "ShaderInclude.h"
#include "Profiling/MemoryStats.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "Platform/Paths.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	constexpr uint64 ShaderCacheFnvOffset = 14695981039346656037ull;
	constexpr uint64 ShaderCacheFnvPrime = 1099511628211ull;
	constexpr const char* ShaderCacheMetaVersion = "LunaticShaderCSOMetaV1";

	struct FShaderCacheMeta
	{
		FString SourcePath;
		FString EntryPoint;
		FString Profile;
		uint64 DefinesHash = 0;
		TArray<FString> Includes;
	};

	struct FShaderBlobCompileRequest
	{
		std::wstring SourcePath;
		FString SourceKey;
		FString EntryPoint;
		FString Profile;
		const D3D_SHADER_MACRO* Defines = nullptr;
		uint64 DefinesHash = 0;
		const char* StageLabel = "Shader";
	};

	uint64 HashBytes(const void* Data, size_t Size, uint64 Seed = ShaderCacheFnvOffset)
	{
		const uint8* Bytes = static_cast<const uint8*>(Data);
		uint64 Hash = Seed;
		for (size_t Index = 0; Index < Size; ++Index)
		{
			Hash ^= Bytes[Index];
			Hash *= ShaderCacheFnvPrime;
		}
		return Hash;
	}

	uint64 HashString(const FString& Value, uint64 Seed)
	{
		return HashBytes(Value.data(), Value.size(), Seed);
	}

	uint64 HashDefinesForCache(const D3D_SHADER_MACRO* Defines)
	{
		if (!Defines)
		{
			return 0;
		}

		uint64 Hash = ShaderCacheFnvOffset;
		for (const D3D_SHADER_MACRO* Define = Defines; Define->Name != nullptr; ++Define)
		{
			const FString Name = Define->Name;
			const FString Value = Define->Definition ? Define->Definition : "";
			Hash = HashString(Name, Hash);
			Hash = HashBytes("=", 1, Hash);
			Hash = HashString(Value, Hash);
			Hash = HashBytes(";", 1, Hash);
		}
		return Hash;
	}

	FString NormalizeMetadataPath(FString Path)
	{
		for (char& Ch : Path)
		{
			if (Ch == '\\')
			{
				Ch = '/';
			}
		}
		return Path;
	}

	std::filesystem::path ToAbsoluteProjectPath(const std::wstring& Path)
	{
		std::filesystem::path FsPath(Path);
		if (FsPath.is_absolute())
		{
			return FsPath.lexically_normal();
		}
		return (std::filesystem::path(FPaths::RootDir()) / FsPath).lexically_normal();
	}

	std::filesystem::path ToAbsoluteProjectPath(const FString& Path)
	{
		return ToAbsoluteProjectPath(FPaths::ToWide(Path));
	}

	FString MakeProjectRelativePath(const std::filesystem::path& AbsolutePath)
	{
		const std::filesystem::path Root = std::filesystem::path(FPaths::RootDir()).lexically_normal();
		std::filesystem::path RelativePath = AbsolutePath.lexically_relative(Root);
		if (RelativePath.empty())
		{
			RelativePath = AbsolutePath.filename();
		}
		return NormalizeMetadataPath(FPaths::ToUtf8(RelativePath.wstring()));
	}

	std::wstring GetShaderCacheDir()
	{
		return FPaths::SaveDir() + L"ShaderCache\\";
	}

	FString ToHex(uint64 Value)
	{
		static constexpr char HexDigits[] = "0123456789abcdef";
		FString Result(16, '0');
		for (int32 Index = 15; Index >= 0; --Index)
		{
			Result[Index] = HexDigits[Value & 0xF];
			Value >>= 4;
		}
		return Result;
	}

	FString SanitizeFileNamePart(FString Value)
	{
		for (char& Ch : Value)
		{
			const unsigned char UCh = static_cast<unsigned char>(Ch);
			if (!std::isalnum(UCh) && Ch != '_' && Ch != '-')
			{
				Ch = '_';
			}
		}
		return Value;
	}

	FString BuildShaderCacheKey(const FShaderBlobCompileRequest& Request)
	{
		uint64 Hash = ShaderCacheFnvOffset;
		Hash = HashString(Request.SourceKey, Hash);
		Hash = HashBytes("|", 1, Hash);
		Hash = HashString(Request.EntryPoint, Hash);
		Hash = HashBytes("|", 1, Hash);
		Hash = HashString(Request.Profile, Hash);
		Hash = HashBytes("|", 1, Hash);
		Hash = HashBytes(&Request.DefinesHash, sizeof(Request.DefinesHash), Hash);
		return ToHex(Hash);
	}

	std::filesystem::path BuildShaderCachePath(const FShaderBlobCompileRequest& Request)
	{
		const FString CacheKey = BuildShaderCacheKey(Request);
		const FString SourceStem = SanitizeFileNamePart(
			NormalizeMetadataPath(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Request.SourceKey)).stem().wstring())));
		const FString Entry = SanitizeFileNamePart(Request.EntryPoint);
		const FString Profile = SanitizeFileNamePart(Request.Profile);
		const FString FileName = SourceStem + "_" + Entry + "_" + Profile + "_" + CacheKey + ".cso";
		return std::filesystem::path(GetShaderCacheDir()) / FPaths::ToWide(FileName);
	}

	std::filesystem::path BuildShaderMetaPath(const std::filesystem::path& CachePath)
	{
		std::filesystem::path MetaPath = CachePath;
		MetaPath += L".meta";
		return MetaPath;
	}

	void AppendUniqueInclude(TArray<FString>& Includes, const FString& IncludePath)
	{
		const FString NormalizedPath = NormalizeMetadataPath(IncludePath);
		if (std::find(Includes.begin(), Includes.end(), NormalizedPath) == Includes.end())
		{
			Includes.push_back(NormalizedPath);
		}
	}

	void AppendIncludes(TArray<FString>* OutIncludes, const TArray<FString>& Includes)
	{
		if (!OutIncludes)
		{
			return;
		}

		for (const FString& IncludePath : Includes)
		{
			AppendUniqueInclude(*OutIncludes, IncludePath);
		}
	}

	bool LoadShaderCacheMeta(const std::filesystem::path& MetaPath, FShaderCacheMeta& OutMeta)
	{
		std::ifstream File(MetaPath, std::ios::binary);
		if (!File)
		{
			return false;
		}

		FString Line;
		if (!std::getline(File, Line) || Line != ShaderCacheMetaVersion)
		{
			return false;
		}

		FShaderCacheMeta Meta;
		while (std::getline(File, Line))
		{
			constexpr const char* SourcePrefix = "source=";
			constexpr const char* EntryPrefix = "entry=";
			constexpr const char* ProfilePrefix = "profile=";
			constexpr const char* DefinesPrefix = "defines=";
			constexpr const char* IncludePrefix = "include=";

			if (Line.rfind(SourcePrefix, 0) == 0)
			{
				Meta.SourcePath = NormalizeMetadataPath(Line.substr(std::strlen(SourcePrefix)));
			}
			else if (Line.rfind(EntryPrefix, 0) == 0)
			{
				Meta.EntryPoint = Line.substr(std::strlen(EntryPrefix));
			}
			else if (Line.rfind(ProfilePrefix, 0) == 0)
			{
				Meta.Profile = Line.substr(std::strlen(ProfilePrefix));
			}
			else if (Line.rfind(DefinesPrefix, 0) == 0)
			{
				Meta.DefinesHash = static_cast<uint64>(_strtoui64(Line.substr(std::strlen(DefinesPrefix)).c_str(), nullptr, 10));
			}
			else if (Line.rfind(IncludePrefix, 0) == 0)
			{
				AppendUniqueInclude(Meta.Includes, Line.substr(std::strlen(IncludePrefix)));
			}
		}

		OutMeta = std::move(Meta);
		return true;
	}

	void SaveShaderCacheMeta(const std::filesystem::path& MetaPath, const FShaderBlobCompileRequest& Request, const TArray<FString>& Includes)
	{
		std::error_code ErrorCode;
		std::filesystem::create_directories(MetaPath.parent_path(), ErrorCode);

		std::ofstream File(MetaPath, std::ios::binary | std::ios::trunc);
		if (!File)
		{
			return;
		}

		TArray<FString> UniqueIncludes;
		for (const FString& IncludePath : Includes)
		{
			AppendUniqueInclude(UniqueIncludes, IncludePath);
		}

		File << ShaderCacheMetaVersion << "\n";
		File << "source=" << NormalizeMetadataPath(Request.SourceKey) << "\n";
		File << "entry=" << Request.EntryPoint << "\n";
		File << "profile=" << Request.Profile << "\n";
		File << "defines=" << Request.DefinesHash << "\n";
		for (const FString& IncludePath : UniqueIncludes)
		{
			File << "include=" << IncludePath << "\n";
		}
	}

	bool TryGetLastWriteTime(const std::filesystem::path& Path, std::filesystem::file_time_type& OutTime)
	{
		std::error_code ErrorCode;
		if (!std::filesystem::exists(Path, ErrorCode))
		{
			return false;
		}

		OutTime = std::filesystem::last_write_time(Path, ErrorCode);
		return !ErrorCode;
	}

	bool IsShaderCacheFresh(const FShaderBlobCompileRequest& Request, const std::filesystem::path& CachePath,
		const std::filesystem::path& MetaPath, FShaderCacheMeta& OutMeta)
	{
		FShaderCacheMeta Meta;
		if (!LoadShaderCacheMeta(MetaPath, Meta))
		{
			return false;
		}

		if (NormalizeMetadataPath(Meta.SourcePath) != NormalizeMetadataPath(Request.SourceKey)
			|| Meta.EntryPoint != Request.EntryPoint
			|| Meta.Profile != Request.Profile
			|| Meta.DefinesHash != Request.DefinesHash)
		{
			return false;
		}

		std::filesystem::file_time_type CacheWriteTime;
		if (!TryGetLastWriteTime(CachePath, CacheWriteTime))
		{
			return false;
		}

		std::filesystem::file_time_type LatestDependencyTime;
		if (!TryGetLastWriteTime(ToAbsoluteProjectPath(Request.SourcePath), LatestDependencyTime))
		{
			return false;
		}

		for (const FString& IncludePath : Meta.Includes)
		{
			std::filesystem::file_time_type IncludeWriteTime;
			const std::filesystem::path IncludeFsPath = std::filesystem::path(FPaths::ShaderDir()) / FPaths::ToWide(IncludePath);
			if (!TryGetLastWriteTime(IncludeFsPath.lexically_normal(), IncludeWriteTime))
			{
				return false;
			}

			if (IncludeWriteTime > LatestDependencyTime)
			{
				LatestDependencyTime = IncludeWriteTime;
			}
		}

		if (LatestDependencyTime > CacheWriteTime)
		{
			return false;
		}

		OutMeta = std::move(Meta);
		return true;
	}

	bool TryLoadCachedShaderBlob(const FShaderBlobCompileRequest& Request, ID3DBlob** OutBlob, TArray<FString>* OutIncludes)
	{
		const std::filesystem::path CachePath = BuildShaderCachePath(Request);
		const std::filesystem::path MetaPath = BuildShaderMetaPath(CachePath);

		FShaderCacheMeta Meta;
		if (!IsShaderCacheFresh(Request, CachePath, MetaPath, Meta))
		{
			return false;
		}

		ID3DBlob* CachedBlob = nullptr;
		if (FAILED(D3DReadFileToBlob(CachePath.c_str(), &CachedBlob)) || !CachedBlob)
		{
			return false;
		}

		AppendIncludes(OutIncludes, Meta.Includes);
		*OutBlob = CachedBlob;
		UE_LOG_CATEGORY(Shader, Debug, "%s CSO cache hit: %s entry=%s profile=%s",
			Request.StageLabel, Request.SourceKey.c_str(), Request.EntryPoint.c_str(), Request.Profile.c_str());
		return true;
	}

	void SaveCachedShaderBlob(const FShaderBlobCompileRequest& Request, ID3DBlob* Blob, const TArray<FString>& Includes)
	{
		if (!Blob)
		{
			return;
		}

		const std::filesystem::path CachePath = BuildShaderCachePath(Request);
		const std::filesystem::path MetaPath = BuildShaderMetaPath(CachePath);

		std::error_code ErrorCode;
		std::filesystem::create_directories(CachePath.parent_path(), ErrorCode);
		if (FAILED(D3DWriteBlobToFile(Blob, CachePath.c_str(), TRUE)))
		{
			return;
		}

		SaveShaderCacheMeta(MetaPath, Request, Includes);
		UE_LOG_CATEGORY(Shader, Debug, "%s CSO cache saved: %s entry=%s profile=%s",
			Request.StageLabel, Request.SourceKey.c_str(), Request.EntryPoint.c_str(), Request.Profile.c_str());
	}

	HRESULT LoadOrCompileShaderBlob(const FShaderBlobCompileRequest& Request, ID3DBlob** OutBlob, ID3DBlob** OutErrorBlob,
		TArray<FString>* OutIncludes)
	{
		*OutBlob = nullptr;
		if (OutErrorBlob)
		{
			*OutErrorBlob = nullptr;
		}

		if (TryLoadCachedShaderBlob(Request, OutBlob, OutIncludes))
		{
			return S_OK;
		}

		TArray<FString> CompileIncludes;
		FShaderInclude IncludeHandler;
		IncludeHandler.OutIncludes = &CompileIncludes;

		ID3DBlob* ErrorBlob = nullptr;
		HRESULT Hr = D3DCompileFromFile(Request.SourcePath.c_str(), Request.Defines, &IncludeHandler,
			Request.EntryPoint.c_str(), Request.Profile.c_str(), 0, 0, OutBlob, &ErrorBlob);

		if (SUCCEEDED(Hr))
		{
			AppendIncludes(OutIncludes, CompileIncludes);
			SaveCachedShaderBlob(Request, *OutBlob, CompileIncludes);
			if (ErrorBlob)
			{
				ErrorBlob->Release();
			}
			return Hr;
		}

		if (OutErrorBlob)
		{
			*OutErrorBlob = ErrorBlob;
		}
		else if (ErrorBlob)
		{
			ErrorBlob->Release();
		}

		return Hr;
	}

	FShaderBlobCompileRequest MakeShaderCompileRequest(const wchar_t* SourcePath, const char* EntryPoint,
		const char* Profile, const D3D_SHADER_MACRO* Defines, const char* StageLabel)
	{
		const std::filesystem::path AbsoluteSourcePath = ToAbsoluteProjectPath(SourcePath);
		FShaderBlobCompileRequest Request;
		Request.SourcePath = AbsoluteSourcePath.wstring();
		Request.SourceKey = MakeProjectRelativePath(AbsoluteSourcePath);
		Request.EntryPoint = EntryPoint ? EntryPoint : "";
		Request.Profile = Profile ? Profile : "";
		Request.Defines = Defines;
		Request.DefinesHash = HashDefinesForCache(Defines);
		Request.StageLabel = StageLabel;
		return Request;
	}
}

// ============================================================
// FComputeShader
// ============================================================

bool FComputeShader::Create(ID3D11Device* InDevice, const wchar_t* Path, const char* EntryPoint,
	TArray<FString>* OutIncludes)
{
	Release();

	if (OutIncludes)
	{
		OutIncludes->clear();
	}

	ID3DBlob* CSBlob = nullptr;
	ID3DBlob* ErrBlob = nullptr;
	const FShaderBlobCompileRequest CompileRequest = MakeShaderCompileRequest(Path, EntryPoint, "cs_5_0", nullptr, "CS");
	HRESULT hr = LoadOrCompileShaderBlob(CompileRequest, &CSBlob, &ErrBlob, OutIncludes);

	if (FAILED(hr))
	{
		if (ErrBlob)
		{
			UE_LOG_CATEGORY(Shader, Error, "CS Compile Error: %s", (const char*)ErrBlob->GetBufferPointer());
			FNotificationManager::Get().AddNotification("CS Compile Error (see log)", ENotificationType::Error, 5.0f);
			ErrBlob->Release();
		}
		return false;
	}

	hr = InDevice->CreateComputeShader(CSBlob->GetBufferPointer(), CSBlob->GetBufferSize(), nullptr, &CS);
	CSBlob->Release();

	return SUCCEEDED(hr) && CS != nullptr;
}

void FComputeShader::Release()
{
	if (CS) { CS->Release(); CS = nullptr; }
}

// ============================================================
// FShader
// ============================================================

FShader::FShader(FShader&& Other) noexcept
	: VertexShader(Other.VertexShader)
	, PixelShader(Other.PixelShader)
	, InputLayout(Other.InputLayout)
	, CachedVertexShaderSize(Other.CachedVertexShaderSize)
	, CachedPixelShaderSize(Other.CachedPixelShaderSize)
	, ShaderParameterLayout(std::move(Other.ShaderParameterLayout))
{
	Other.VertexShader = nullptr;
	Other.PixelShader = nullptr;
	Other.InputLayout = nullptr;
	Other.CachedVertexShaderSize = 0;
	Other.CachedPixelShaderSize = 0;
}

FShader& FShader::operator=(FShader&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		VertexShader = Other.VertexShader;
		PixelShader = Other.PixelShader;
		InputLayout = Other.InputLayout;
		CachedVertexShaderSize = Other.CachedVertexShaderSize;
		CachedPixelShaderSize = Other.CachedPixelShaderSize;
		ShaderParameterLayout = std::move(Other.ShaderParameterLayout);
		Other.VertexShader = nullptr;
		Other.PixelShader = nullptr;
		Other.InputLayout = nullptr;
		Other.CachedVertexShaderSize = 0;
		Other.CachedPixelShaderSize = 0;
	}
	return *this;
}

void FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
	const D3D_SHADER_MACRO* InDefines, TArray<FString>* OutIncludes, EShaderErrorMode ErrorMode)
{
	Release();
	if (OutIncludes)
	{
		OutIncludes->clear();
	}

	ID3DBlob* vertexShaderCSO = nullptr;
	ID3DBlob* pixelShaderCSO = nullptr;
	ID3DBlob* errorBlob = nullptr;

	// Vertex Shader 컴파일
	const FShaderBlobCompileRequest VSCompileRequest = MakeShaderCompileRequest(InFilePath, InVSEntryPoint, "vs_5_0", InDefines, "VS");
	HRESULT hr = LoadOrCompileShaderBlob(VSCompileRequest, &vertexShaderCSO, &errorBlob, OutIncludes);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			const char* Msg = (const char*)errorBlob->GetBufferPointer();
			UE_LOG_CATEGORY(Shader, Error, "VS Compile Error: %s", Msg);
			if (ErrorMode == EShaderErrorMode::MessageBox)
				MessageBoxA(nullptr, Msg, "VS Compile Error", MB_OK | MB_ICONERROR);
			else
				FNotificationManager::Get().AddNotification("VS Compile Error (see log)", ENotificationType::Error, 5.0f);
			errorBlob->Release();
		}
		return;
	}
	if (errorBlob)
	{
		errorBlob->Release();
		errorBlob = nullptr;
	}

	// Pixel Shader 컴파일
	const FShaderBlobCompileRequest PSCompileRequest = MakeShaderCompileRequest(InFilePath, InPSEntryPoint, "ps_5_0", InDefines, "PS");
	hr = LoadOrCompileShaderBlob(PSCompileRequest, &pixelShaderCSO, &errorBlob, OutIncludes);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			const char* Msg = (const char*)errorBlob->GetBufferPointer();
			UE_LOG_CATEGORY(Shader, Error, "PS Compile Error: %s", Msg);
			if (ErrorMode == EShaderErrorMode::MessageBox)
				MessageBoxA(nullptr, Msg, "PS Compile Error", MB_OK | MB_ICONERROR);
			else
				FNotificationManager::Get().AddNotification("PS Compile Error (see log)", ENotificationType::Error, 5.0f);
			errorBlob->Release();
		}
		vertexShaderCSO->Release();
		return;
	}

	// Vertex Shader 생성
	hr = InDevice->CreateVertexShader(vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), nullptr, &VertexShader);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Vertex Shader (HRESULT: " << hr << ")" << std::endl;
		vertexShaderCSO->Release();
		pixelShaderCSO->Release();
		return;
	}

	CachedVertexShaderSize = vertexShaderCSO->GetBufferSize();
	MemoryStats::AddVertexShaderMemory(static_cast<uint32>(CachedVertexShaderSize));

	// Pixel Shader 생성
	hr = InDevice->CreatePixelShader(pixelShaderCSO->GetBufferPointer(), pixelShaderCSO->GetBufferSize(), nullptr, &PixelShader);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Pixel Shader (HRESULT: " << hr << ")" << std::endl;
		Release();
		vertexShaderCSO->Release();
		pixelShaderCSO->Release();
		return;
	}

	CachedPixelShaderSize = pixelShaderCSO->GetBufferSize();
	MemoryStats::AddPixelShaderMemory(static_cast<uint32>(CachedPixelShaderSize));

	// Input Layout 생성 (VS input signature로부터 자동 추출)
	CreateInputLayoutFromReflection(InDevice, vertexShaderCSO);

	ExtractCBufferInfo(vertexShaderCSO, ShaderParameterLayout);
	ExtractCBufferInfo(pixelShaderCSO, ShaderParameterLayout);

	vertexShaderCSO->Release();
	pixelShaderCSO->Release();
}

void FShader::Release()
{
	if (InputLayout)
	{
		InputLayout->Release();
		InputLayout = nullptr;
	}
	if (PixelShader)
	{
		MemoryStats::SubPixelShaderMemory(static_cast<uint32>(CachedPixelShaderSize));
		CachedPixelShaderSize = 0;

		PixelShader->Release();
		PixelShader = nullptr;
	}
	if (VertexShader)
	{
		MemoryStats::SubVertexShaderMemory(static_cast<uint32>(CachedVertexShaderSize));
		CachedVertexShaderSize = 0;

		VertexShader->Release();
		VertexShader = nullptr;
	}
}

void FShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
	InDeviceContext->IASetInputLayout(InputLayout);
	InDeviceContext->VSSetShader(VertexShader, nullptr, 0);
	InDeviceContext->PSSetShader(PixelShader, nullptr, 0);
}


namespace
{
	DXGI_FORMAT MaskToFormat(D3D_REGISTER_COMPONENT_TYPE ComponentType, BYTE Mask)
	{
		// Mask 비트 수 세기 (사용되는 컴포넌트 개수)
		int Count = 0;
		if (Mask & 0x1) ++Count;
		if (Mask & 0x2) ++Count;
		if (Mask & 0x4) ++Count;
		if (Mask & 0x8) ++Count;

		if (ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
		{
			switch (Count)
			{
			case 1: return DXGI_FORMAT_R32_FLOAT;
			case 2: return DXGI_FORMAT_R32G32_FLOAT;
			case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
			case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
		}
		else if (ComponentType == D3D_REGISTER_COMPONENT_UINT32)
		{
			switch (Count)
			{
			case 1: return DXGI_FORMAT_R32_UINT;
			case 2: return DXGI_FORMAT_R32G32_UINT;
			case 3: return DXGI_FORMAT_R32G32B32_UINT;
			case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
			}
		}
		else if (ComponentType == D3D_REGISTER_COMPONENT_SINT32)
		{
			switch (Count)
			{
			case 1: return DXGI_FORMAT_R32_SINT;
			case 2: return DXGI_FORMAT_R32G32_SINT;
			case 3: return DXGI_FORMAT_R32G32B32_SINT;
			case 4: return DXGI_FORMAT_R32G32B32A32_SINT;
			}
		}
		return DXGI_FORMAT_UNKNOWN;
	}
}

void FShader::CreateInputLayoutFromReflection(ID3D11Device* InDevice, ID3DBlob* VSBlob)
{
	ID3D11ShaderReflection* Reflector = nullptr;
	HRESULT hr = D3DReflect(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection, (void**)&Reflector);
	if (FAILED(hr)) return;

	D3D11_SHADER_DESC ShaderDesc;
	Reflector->GetDesc(&ShaderDesc);

	TArray<D3D11_INPUT_ELEMENT_DESC> Elements;

	for (UINT i = 0; i < ShaderDesc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
		Reflector->GetInputParameterDesc(i, &ParamDesc);

		// SV_VertexID, SV_InstanceID 등 시스템 시맨틱은 스킵
		if (ParamDesc.SystemValueType != D3D_NAME_UNDEFINED)
			continue;

		D3D11_INPUT_ELEMENT_DESC Elem = {};
		Elem.SemanticName = ParamDesc.SemanticName;
		Elem.SemanticIndex = ParamDesc.SemanticIndex;
		Elem.Format = MaskToFormat(ParamDesc.ComponentType, ParamDesc.Mask);
		Elem.InputSlot = 0;
		Elem.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		Elem.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		Elem.InstanceDataStepRate = 0;

		Elements.push_back(Elem);
	}

	// Fullscreen quad 등 vertex input이 없는 셰이더는 InputLayout 불필요
	if (Elements.empty())
	{
		Reflector->Release();
		return;
	}

	hr = InDevice->CreateInputLayout(Elements.data(), (UINT)Elements.size(),
		VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &InputLayout);
	if (FAILED(hr))
	{
		std::cerr << "Failed to create Input Layout from reflection (HRESULT: " << hr << ")" << std::endl;
	}

	Reflector->Release();
}

//셰이더 컴파일 후 호출. 셰이더 코드의 cbuffer, 텍스처 샘플러 선언을 분석해서 outlayout에 채워넣음. 이 정보는 머티리얼 템플릿이 생성될 때 참조되어야 하므로 셰이더 내부에서 제공하는 형태로 존재해야 함.
void FShader::ExtractCBufferInfo(ID3DBlob* ShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout)
{
	ID3D11ShaderReflection* Reflector = nullptr;
	D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection, (void**)&Reflector);

	D3D11_SHADER_DESC ShaderDesc;
	Reflector->GetDesc(&ShaderDesc);

	for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
	{
		auto* CB = Reflector->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC CBDesc;
		CB->GetDesc(&CBDesc);

		FString BufferName = CBDesc.Name;  // "PerMaterial", "PerFrame" 등

		//상수 버퍼의 바인딩 정보(Slot Index) 가져오기
		D3D11_SHADER_INPUT_BIND_DESC BindDesc;
		Reflector->GetResourceBindingDescByName(CBDesc.Name, &BindDesc);
		UINT SlotIndex = BindDesc.BindPoint; // 이것이 b0, b1의 숫자입니다.

		if (SlotIndex != 2 && SlotIndex != 3)  // b2, b3만 저장
			continue;

		for (UINT j = 0; j < CBDesc.Variables; ++j)
		{
			auto* Var = CB->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC VarDesc;
			Var->GetDesc(&VarDesc);

			FMaterialParameterInfo* Info = new FMaterialParameterInfo();
			Info->BufferName = BufferName;
			Info->SlotIndex = SlotIndex;
			Info->Offset = VarDesc.StartOffset;
			Info->Size = VarDesc.Size;
			
			Info->BufferSize = CBDesc.Size;//cbuffer 크기

			OutLayout[VarDesc.Name] = Info;
		}
	}
	Reflector->Release();
}

