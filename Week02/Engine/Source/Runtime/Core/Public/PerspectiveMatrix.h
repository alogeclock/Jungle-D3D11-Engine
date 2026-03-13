#pragma once

#include "UnrealMathUtility.h"
#include "Plane.h"
#include "Matrix.h"

template<typename T>
struct FPerspectiveMatrix
	:public FMatrix
{
#define Z_PRECISION	0.0f

	FPerspectiveMatrix(T HalfFOVX, T HalfFOVY, T MultFOVX, T MultFOVY, T MinZ, T MaxZ);
};

/********************************************/

template<typename T>
FPerspectiveMatrix<T>::FPerspectiveMatrix(T HalfFOVX, T HalfFOVY, T MultFOVX, T MultFOVY, T MinZ, T MaxZ)
	: FMatrix(
		FPlane<T>(MultFOVX / FMath::Tan(HalfFOVX),	0.0f,								0.0f,																	0.0f),
		FPlane<T>(0.0f,								MultFOVY / FMath::Tan(HalfFOVY),	0.0f,																	0.0f),
		FPlane<T>(0.0f,								0.0f,								((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)),			1.0f),
		FPlane<T>(0.0f,								0.0f,								-MinZ * ((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)),	0.0f)
	)
{ }