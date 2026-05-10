#include "SkeletalMeshPreviewViewportClient.h"

#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Object/Object.h"

static bool GetSkeletalMeshBounds(USkeletalMesh* SkeletalMesh, FVector& OutCenter, FVector& OutExtent);

FSkeletalMeshPreviewViewportClient::FSkeletalMeshPreviewViewportClient()
{
	CreatePreviewComponent();
}

FSkeletalMeshPreviewViewportClient::~FSkeletalMeshPreviewViewportClient()
{
	DestroyPreviewComponent();
}

void FSkeletalMeshPreviewViewportClient::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	PreviewSkeletalMesh = InSkeletalMesh;
	CreatePreviewComponent();

	if (PreviewComponent)
	{
		PreviewComponent->SkeletalMeshAsset = PreviewSkeletalMesh;
		PreviewComponent->SkinnedAsset = PreviewSkeletalMesh;
		PreviewComponent->SkeletalMeshAssetPath = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetAssetPathFileName() : "None";
		PreviewComponent->SkinnedMeshAssetPath = PreviewComponent->SkeletalMeshAssetPath;
		PreviewComponent->bRequiredBonesUpdated = true;
		PreviewComponent->bSkinningDirty = true;
		PreviewComponent->bPoseDirty = true;
		PreviewComponent->bBoundsDirty = true;
	}

	FocusPreviewMesh();
}

bool FSkeletalMeshPreviewViewportClient::FocusPreviewMesh()
{
	FVector Center = FVector::ZeroVector;
	FVector Extent = FVector(1.0f, 1.0f, 1.0f);
	if (!GetSkeletalMeshBounds(PreviewSkeletalMesh, Center, Extent))
	{
		return FocusBounds(Center, Extent);
	}

	return FocusBounds(Center, Extent);
}

void FSkeletalMeshPreviewViewportClient::OnCameraReset()
{
	FocusPreviewMesh();
}

void FSkeletalMeshPreviewViewportClient::CreatePreviewComponent()
{
	if (!PreviewComponent)
	{
		PreviewComponent = UObjectManager::Get().CreateObject<USkeletalMeshComponent>();
	}
}

void FSkeletalMeshPreviewViewportClient::DestroyPreviewComponent()
{
	if (PreviewComponent)
	{
		UObjectManager::Get().DestroyObject(PreviewComponent);
		PreviewComponent = nullptr;
	}
}

static bool GetSkeletalMeshBounds(USkeletalMesh* SkeletalMesh, FVector& OutCenter, FVector& OutExtent)
{
	if (!SkeletalMesh)
	{
		return false;
	}

	const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!MeshAsset)
	{
		return false;
	}

	const FSkeletalMeshLOD* LOD = MeshAsset->GetLOD(0);
	if (!LOD || !LOD->bBoundsValid)
	{
		return false;
	}

	OutCenter = LOD->BoundsCenter;
	OutExtent = LOD->BoundsExtent;
	return true;
}
