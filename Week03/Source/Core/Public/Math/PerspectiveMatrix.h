#pragma once

#include "UnrealMathUtility.h"
#include "Plane.h"
#include "Matrix.h"

template<typename T>
struct FPerspectiveMatrix
	:public FMatrix<T>
{
#define Z_PRECISION	0.0f

	FPerspectiveMatrix(T HalfFOVX, T HalfFOVY, T MultFOVX, T MultFOVY, T MinZ, T MaxZ);
    FMatrix<T> Inverse() const;
};

/********************************************/

template<typename T>
FPerspectiveMatrix<T>::FPerspectiveMatrix(T HalfFOVX, T HalfFOVY, T MultFOVX, T MultFOVY, T MinZ, T MaxZ)
	: FMatrix<T>(
		FPlane<T>(MultFOVX / FMath::Tan(HalfFOVX),	0.0f,								0.0f,																	0.0f),
		FPlane<T>(0.0f,								MultFOVY / FMath::Tan(HalfFOVY),	0.0f,																	0.0f),
		FPlane<T>(0.0f,								0.0f,								((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)),			1.0f),
		FPlane<T>(0.0f,								0.0f,								-MinZ * ((MinZ == MaxZ) ? (1.0f - Z_PRECISION) : MaxZ / (MaxZ - MinZ)),	0.0f)
	)
{
}

template <typename T> 
FMatrix<T> FPerspectiveMatrix<T>::Inverse() const 
{ 
	// M[0][0] = a, M[1][1] = b, M[2][2] = c, M[3][2] = d(=-MinZ*c), M[2][3] = 1
    const float a = this->M[0][0];
    const float b = this->M[1][1];
    const float c = this->M[2][2]; // MaxZ / (MaxZ - MinZ)
    const float d = this->M[3][2]; // -MinZ * c

	FMatrix<T> Result = FMatrix<T>::Identity();

    // 역행렬:
    // [ 1/a   0    0      0   ]
    // [  0   1/b   0      0   ]
    // [  0    0    0     1/d  ]
    // [  0    0    1    -c/d  ]

	Result.M[0][0] = 1.f / a;
    Result.M[1][1] = 1.f / b;
    Result.M[2][2] = 0.f;
    Result.M[2][3] = 1.f / d; // d = -MinZ * c 이므로 Z 복원
    Result.M[3][2] = 1.f;
    Result.M[3][3] = -c / d;

    return Result;
}
