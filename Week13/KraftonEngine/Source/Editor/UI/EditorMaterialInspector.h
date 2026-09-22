#pragma once
#include <filesystem>
#include <fstream>
#include <Engine/Materials/MaterialManager.h>
#include <wrl/client.h>
#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"


struct ID3D11ShaderResourceView;

class FEditorMaterialInspector final
{
public:
	FEditorMaterialInspector() = default;
	FEditorMaterialInspector(std::filesystem::path InPath);
	void Render();

private:
	void RenderShaderParameter();
	void RenderTextureSection();

private:
	std::filesystem::path MaterialPath;
	json::JSON CachedJson;
	UMaterial* CachedMaterial;

	TMap<FString, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> CachedSRVs;
};

