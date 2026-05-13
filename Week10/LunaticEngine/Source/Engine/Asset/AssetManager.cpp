#include "Asset/AssetManager.h"

#include "Mesh/SkeletalMeshManager.h"

USkeletalMesh* FAssetManager::LoadSkeletalMesh(const FAssetReference& Reference)
{
	if (Reference.IsNull())
	{
		return nullptr;
	}

	return FSkeletalMeshManager::LoadSkeletalMesh(Reference.Path);
}
