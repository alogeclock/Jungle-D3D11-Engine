#include "SkeletalMeshPreviewViewportClient.h"

#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/SkeletalMesh.h"

namespace
{
	bool GetSkeletalMeshBounds(USkeletalMesh* SkeletalMesh, FVector& OutCenter, FVector& OutExtent);
}

FSkeletalMeshPreviewViewportClient::FSkeletalMeshPreviewViewportClient()
{
}

FSkeletalMeshPreviewViewportClient::~FSkeletalMeshPreviewViewportClient()
{
	DestroyPreviewComponent();
}

void FSkeletalMeshPreviewViewportClient::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	PreviewSkeletalMesh = InSkeletalMesh;
	if (!PreviewSkeletalMesh)
	{
		DestroyPreviewComponent();
		return;
	}

	CreatePreviewComponent();
	if (PreviewComponent)
	{
		PreviewComponent->SetSkeletalMesh(PreviewSkeletalMesh);
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
	if (PreviewComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PreviewActor = GetPreviewScene().SpawnPreviewActor();
	if (!PreviewActor)
	{
		return;
	}

	PreviewComponent = PreviewActor->AddComponent<USkeletalMeshComponent>();
	PreviewActor->SetRootComponent(PreviewComponent);
}

void FSkeletalMeshPreviewViewportClient::DestroyPreviewComponent()
{
	if (PreviewActor)
	{
		GetPreviewScene().DestroyPreviewActor(PreviewActor);
	}

	PreviewActor = nullptr;
	PreviewComponent = nullptr;
}

namespace
{
	bool GetSkeletalMeshBounds(USkeletalMesh* SkeletalMesh, FVector& OutCenter, FVector& OutExtent)
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
}
