#pragma once

#include "Core/CoreTypes.h"

struct ID3D11Device;

struct FEditorAssetPipelineReport
{
	uint32 Scanned = 0;
	uint32 Imported = 0;
	uint32 Skipped = 0;
	uint32 Failed = 0;
};

struct FEditorAssetPipeline
{
	static FEditorAssetPipelineReport SyncAssetRoot(ID3D11Device* Device);
};
