#pragma once

#include "Vector.h"
#include "Plane.h"
#include "Matrix.h"

template<typename T>
struct FTranslationMatrix : public FMatrix<T>
{
public:
	FTranslationMatrix(const FVector& Delta);
};

/********************************************/

template<typename T>
FTranslationMatrix<T>::FTranslationMatrix(const FVector& Delta)
	: FMatrix<T>(
		FPlane<T>(1.0f,		0.0f,		0.0f,		0.0f),
		FPlane<T>(0.0f,		1.0f,		0.0f,		0.0f),
		FPlane<T>(0.0f,		0.0f,		1.0f,		0.0f),
		FPlane<T>(Delta.X,	Delta.Y,	Delta.Z,	1.0f)
	)
{ }