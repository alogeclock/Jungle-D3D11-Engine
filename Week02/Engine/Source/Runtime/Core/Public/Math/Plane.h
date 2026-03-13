#pragma once

#include "Vector.h"
#include "Vector4.h"

template<typename T>
struct FPlane
	: public FVector<T>
{
public:
	T W;

	FPlane(T InX, T InY, T InZ, T InW);
	FPlane(const FVector4<T>& V);
};

/********************************************/


template<typename T>
inline FPlane<T>::FPlane(T InX, T InY, T InZ, T InW)
	: FVector<T>(InX, InY, InZ)
	, W(InW)
{
}

template<typename T>
inline FPlane<T>::FPlane(const FVector4<T>& V)
	: FVector<T>(V)
	, W(V.W)
{
}
