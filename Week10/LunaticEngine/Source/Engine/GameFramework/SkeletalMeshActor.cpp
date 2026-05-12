#include "GameFramework/SkeletalMeshActor.h"

#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
	SkeletalMeshComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(SkeletalMeshComponent);

	if (!SkeletalMeshFileName.empty() && SkeletalMeshFileName != "None")
	{
		SkeletalMeshComponent->SetSkeletalMesh(FSkeletalMeshManager::LoadSkeletalMesh(SkeletalMeshFileName));
	}
	else
	{
		SkeletalMeshComponent->SetSkeletalMesh(nullptr);
	}
}
