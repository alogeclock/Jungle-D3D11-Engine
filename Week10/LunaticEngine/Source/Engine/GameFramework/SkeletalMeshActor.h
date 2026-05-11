#pragma once

#include "GameFramework/AActor.h"

class USkeletalMeshComponent;

class ASkeletalMeshActor : public AActor
{
public:
	DECLARE_CLASS(ASkeletalMeshActor, AActor)

	ASkeletalMeshActor() = default;

	void InitDefaultComponents(const FString& SkeletalMeshFileName = "None");
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

private:
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};
