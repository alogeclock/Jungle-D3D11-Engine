#pragma once

#include "../Source/Runtime/Core/Public/CoreMinimal.h"
#include "Actor.h"
#include "../Source/Runtime/CoreUObject/Class.h"

struct FActorSpawnParameters
{

};

class UWorld
{
/**
 * 주어진 transform과 SpawnParameters를 사용하여 Actor를 Spawn 합니다
 *
 * @param	Class					Spawn할 클래스
 * @param	Location				Spawn할 위치
 * @param	Rotation				Spawn할 회전값
 * @param	SpawnParameters			Spawn 파라미터
 *
 * @return	방금 spawn된 Actor
 */
	AActor* SpawnActor(UClass* InClass, DVector const* Location = NULL, DRotator const* Rotation = NULL, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
};
