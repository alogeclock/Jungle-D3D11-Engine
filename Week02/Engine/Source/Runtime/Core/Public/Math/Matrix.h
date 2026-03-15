// Math/Matrix.h
#pragma once
#include "Plane.h"

template<typename T>
struct FMatrix
{
public:
	float M[4][4];

	FMatrix() = default;
	FMatrix(const FPlane<T>& InX, const FPlane<T>& InY, const FPlane<T>& InZ, const FPlane<T>& InW);

	static FMatrix<T> Identity();

	FMatrix<T> operator*(const FMatrix& Other) const;
	FMatrix<T>& operator=(const FMatrix& Other);

	float      Determinant() const;
    FMatrix<T> Inverse() const;
};

template<typename T>
FMatrix<T>::FMatrix(const FPlane<T>& InX, const FPlane<T>& InY, const FPlane<T>& InZ, const FPlane<T>& InW)
{
	M[0][0] = (float)InX.X; M[0][1] = (float)InX.Y; M[0][2] = (float)InX.Z; M[0][3] = (float)InX.W;
	M[1][0] = (float)InY.X; M[1][1] = (float)InY.Y; M[1][2] = (float)InY.Z; M[1][3] = (float)InY.W;
	M[2][0] = (float)InZ.X; M[2][1] = (float)InZ.Y; M[2][2] = (float)InZ.Z; M[2][3] = (float)InZ.W;
	M[3][0] = (float)InW.X; M[3][1] = (float)InW.Y; M[3][2] = (float)InW.Z; M[3][3] = (float)InW.W;
}

template<typename T>
inline FMatrix<T> FMatrix<T>::Identity()
{
	// 단위 행렬
	FMatrix result;

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			result.M[i][j] = (i == j) ? 1.0f : 0.0f;
		}
	}

	return result;
}

template<typename T>
inline FMatrix<T> FMatrix<T>::operator*(const FMatrix& Other) const
{
	FMatrix Result;

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			Result.M[i][j] = 0.0f;

			for (int k = 0; k < 4; ++k)
			{
				Result.M[i][j] += M[i][k] * Other.M[k][j];
			}
		}
	}

	return Result;
}

template<typename T>
inline FMatrix<T>& FMatrix<T>::operator=(const FMatrix& Other) 
{
	if (this != &Other) // 자기 자신에 대입하는 경우 방지
	{
		for (int Row = 0; Row < 4; ++Row)
		{
			for (int Col = 0; Col < 4; ++Col)
			{
				M[Row][Col] = Other.M[Row][Col];
			}
		}
	}
	return *this;
}

// 행렬식
template <typename T> inline float FMatrix<T>::Determinant() const 
{
	// 소행렬식
    auto Det3x3 = [](
		float a00, float a01, float a02, 
		float a10, float a11, float a12, 
		float a20, float a21, float a22) -> float
    { 
			return a00 * (a11 * a22 - a12 * a21) 
				- a01 * (a10 * a22 - a12 * a20) 
				+ a02 * (a10 * a21 - a11 * a20); 
	};

    float Det = 0.f;

    // M[0][j] 기준 여인수 전개
    Det += M[0][0] * Det3x3(
		M[1][1], M[1][2], M[1][3], 
		M[2][1], M[2][2], M[2][3], 
		M[3][1], M[3][2], M[3][3]);

    Det -= M[0][1] * Det3x3(
		M[1][0], M[1][2], M[1][3], 
		M[2][0], M[2][2], M[2][3], 
		M[3][0], M[3][2], M[3][3]);

    Det += M[0][2] * Det3x3(
		M[1][0], M[1][1], M[1][3], 
		M[2][0], M[2][1], M[2][3], 
		M[3][0], M[3][1], M[3][3]);

    Det -= M[0][3] * Det3x3(
		M[1][0], M[1][1], M[1][2], 
		M[2][0], M[2][1], M[2][2], 
		M[3][0], M[3][1], M[3][2]);

    return Det;
}

// 역행렬
template <typename T> inline FMatrix<T> FMatrix<T>::Inverse() const 
{
	auto Det3x3 = [](
        float a00, float a01, float a02,
        float a10, float a11, float a12,
        float a20, float a21, float a22) -> float
    {
        return a00 * (a11 * a22 - a12 * a21)
             - a01 * (a10 * a22 - a12 * a20)
             + a02 * (a10 * a21 - a11 * a20);
    };

	// 각 원소의 여인수(Cofactor) 계산 후 전치 = 수반행렬(Adjugate)

	FMatrix<T> Result;

    Result.M[0][0] = +Det3x3(M[1][1], M[1][2], M[1][3], M[2][1], M[2][2], M[2][3], M[3][1], M[3][2], M[3][3]);
    Result.M[1][0] = -Det3x3(M[1][0], M[1][2], M[1][3], M[2][0], M[2][2], M[2][3], M[3][0], M[3][2], M[3][3]);
    Result.M[2][0] = +Det3x3(M[1][0], M[1][1], M[1][3], M[2][0], M[2][1], M[2][3], M[3][0], M[3][1], M[3][3]);
    Result.M[3][0] = -Det3x3(M[1][0], M[1][1], M[1][2], M[2][0], M[2][1], M[2][2], M[3][0], M[3][1], M[3][2]);

    Result.M[0][1] = -Det3x3(M[0][1], M[0][2], M[0][3], M[2][1], M[2][2], M[2][3], M[3][1], M[3][2], M[3][3]);
    Result.M[1][1] = +Det3x3(M[0][0], M[0][2], M[0][3], M[2][0], M[2][2], M[2][3], M[3][0], M[3][2], M[3][3]);
    Result.M[2][1] = -Det3x3(M[0][0], M[0][1], M[0][3], M[2][0], M[2][1], M[2][3], M[3][0], M[3][1], M[3][3]);
    Result.M[3][1] = +Det3x3(M[0][0], M[0][1], M[0][2], M[2][0], M[2][1], M[2][2], M[3][0], M[3][1], M[3][2]);

    Result.M[0][2] = +Det3x3(M[0][1], M[0][2], M[0][3], M[1][1], M[1][2], M[1][3], M[3][1], M[3][2], M[3][3]);
    Result.M[1][2] = -Det3x3(M[0][0], M[0][2], M[0][3], M[1][0], M[1][2], M[1][3], M[3][0], M[3][2], M[3][3]);
    Result.M[2][2] = +Det3x3(M[0][0], M[0][1], M[0][3], M[1][0], M[1][1], M[1][3], M[3][0], M[3][1], M[3][3]);
    Result.M[3][2] = -Det3x3(M[0][0], M[0][1], M[0][2], M[1][0], M[1][1], M[1][2], M[3][0], M[3][1], M[3][2]);

    Result.M[0][3] = -Det3x3(M[0][1], M[0][2], M[0][3], M[1][1], M[1][2], M[1][3], M[2][1], M[2][2], M[2][3]);
    Result.M[1][3] = +Det3x3(M[0][0], M[0][2], M[0][3], M[1][0], M[1][2], M[1][3], M[2][0], M[2][2], M[2][3]);
    Result.M[2][3] = -Det3x3(M[0][0], M[0][1], M[0][3], M[1][0], M[1][1], M[1][3], M[2][0], M[2][1], M[2][3]);
    Result.M[3][3] = +Det3x3(M[0][0], M[0][1], M[0][2], M[1][0], M[1][1], M[1][2], M[2][0], M[2][1], M[2][2]);

	const float Det 
		= M[0][0] * Result.M[0][0] 
		+ M[0][1] * Result.M[1][0] 
		+ M[0][2] * Result.M[2][0] 
		+ M[0][3] * Result.M[3][0];

	// Singular Matrix 체크 (행렬식 0이면 역행렬 없음)
    if (fabs(Det) < 1e-6f)
    {
        return FMatrix<T>::Identity();
    }

	// Inverse = Adjugate / Determinant
	const float InvDet = 1.f / Det;
    for (int32 i = 0; i < 4; i++)
        for (int32 j = 0; j < 4; j++)
            Result.M[i][j] *= InvDet;

    return Result;
}
