#pragma once

#include "Vector.h"

template<typename T>
struct FPlane
	: public FVector
{
public:
	T W;

	FPlane(T InX, T InY, T InZ, T InW);
};

/********************************************/

template<typename T>
FPlane<T>::FPlane(T InX, T InY, T InZ, T InW)
	: FVector(InX, InY, InZ)
	, W(InW)
{
}
