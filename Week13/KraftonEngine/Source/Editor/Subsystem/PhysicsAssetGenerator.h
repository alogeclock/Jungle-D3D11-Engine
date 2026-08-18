#pragma once

#include "Editor/Subsystem/AssetFactory.h"

class FPhysicsAssetGenerator
{
public:
	static bool PopulatePhysicsAssetFromSkeletalMesh(
		UPhysicsAsset* PhysicsAsset,
		const FString& SkeletalMeshPath,
		const FPhysicsAssetCreateParams& CreateParams,
		ID3D11Device* Device);

	static const FString& GetLastPhysicsAssetCreateWarning();
};
