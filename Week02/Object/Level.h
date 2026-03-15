#pragma once

#include "Object.h"

class AActor;

class ULevel : public UObject
{
public:
	TArray<AActor *> Actors;
	//TArray<TObjectPtr<AActor>> Actors;
};
