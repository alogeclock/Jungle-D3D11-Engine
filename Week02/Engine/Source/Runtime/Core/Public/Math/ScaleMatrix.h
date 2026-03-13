#pragma once

#include "Plane.h"
#include "Matrix.h"

template<typename T>
struct FScaleMatrix
	: public FMatrix<T>
{
	FScaleMatrix(const FVector<T>& Scale);
};


/********************************************/


template<typename T>
FScaleMatrix<T>::FScaleMatrix(const FVector& Scale)
	: FMatrix<T>(
		FPlane<T>(Scale.X,	0.0f,		0.0f,		0.0f),
		FPlane<T>(0.0f,		Scale.Y,	0.0f,		0.0f),
		FPlane<T>(0.0f,		0.0f,		Scale.Z,	0.0f),
		FPlane<T>(0.0f,		0.0f,		0.0f,		1.0f)
	)
{ }