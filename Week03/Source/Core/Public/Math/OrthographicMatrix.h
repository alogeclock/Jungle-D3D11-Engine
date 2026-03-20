#pragma once

#include "UnrealMathUtility.h"
#include "Plane.h"
#include "Matrix.h"

template <typename T> struct FOrthographicMatrix : public FMatrix<T>
{
#define Z_PRECISION 0.0f

    FOrthographicMatrix(T HalfWidth, T HalfHeight, T MinZ, T MaxZ);
    FMatrix<T> Inverse() const;
};

/********************************************/

template <typename T>
FOrthographicMatrix<T>::FOrthographicMatrix(T HalfWidth, T HalfHeight, T MinZ, T MaxZ)
    : FMatrix<T>(
        FPlane<T>(1.0f / HalfWidth, 0.0f,              0.0f,                  0.0f), 
        FPlane<T>(0.0f,             1.0f / HalfHeight, 0.0f,                  0.0f),
        FPlane<T>(0.0f,             0.0f,              1.0f / (MaxZ - MinZ),  0.0f), 
        FPlane<T>(0.0f,             0.0f,              -MinZ / (MaxZ - MinZ), 1.0f))
{
}