#pragma once

#include "Vector.h"

template<typename T>
struct FPlane
	: public FVector<T>
{
public:
	T W;

	FPlane(T InX, T InY, T InZ, T InW);
};

/********************************************/

template<typename T>
inline FPlane<T>::FPlane(T InX, T InY, T InZ, T InW)
	: FVector<T>(InX, InY, InZ)
	, W(InW)
{
}