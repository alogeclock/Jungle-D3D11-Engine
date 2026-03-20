#pragma once

#include "Source/Runtime/Core/Public/Math/UnrealMathUtility.h"
#include "Vector.h"
#include "Matrix.h"

template<typename T>
struct FVector4
{
public:
    // ---------------------------------------------------------
    // 1. 멤버 변수 (Member Variables)
    // ---------------------------------------------------------
    T X;
    T Y;
    T Z;
    T W;

    // ---------------------------------------------------------
    // 2. 생성자 (Constructors)
    // ---------------------------------------------------------
    FVector4() = default;
    FVector4(FVector<T> InVector);
    FVector4(T InX, T InY, T InZ, T InW);

    // ---------------------------------------------------------
    // 3. 일반 사칙 연산자 (Basic Math Operators)
    // ---------------------------------------------------------
    FVector4<T> operator+(const FVector4<T>& V) const;
    FVector4<T> operator-(const FVector4<T>& V) const;
    FVector4<T> operator*(T Scale) const;
    FVector4<T> operator*(const FMatrix<T> &M) const;
    FVector4<T> operator/(T Scale) const;

    // ---------------------------------------------------------
    // 4. 복합 대입 연산자 (Assignment Operators)
    // ---------------------------------------------------------
    FVector4<T>& operator+=(const FVector4<T>& V);
    FVector4<T>& operator-=(const FVector4<T>& V); // +=이 있다면 -=도 짝꿍으로 필수!
    FVector4<T>& operator*=(T Scale);
    FVector4<T>& operator/=(T Scale);

    // ---------------------------------------------------------
    // 5. 인스턴스 유틸리티 함수 (Instance Utility Functions)
    // ---------------------------------------------------------
    T SizeSquared() const;
    T Size() const;
    T Length() const;
    void Normalize();

    // ---------------------------------------------------------
    // 6. 공용 수학 계산기 (Static Math Functions)
    // ---------------------------------------------------------
    T Dot4(const FVector4<T>& V1, const FVector4<T>& V2); // dot product
    FVector4<T> operator^(const FVector4<T>& V) const; // cross product
};

// =========================================================
// 생성자
// =========================================================
template<typename T>
inline FVector4<T>::FVector4(FVector<T> InVector)
    : X(InVector.X)
    , Y(InVector.Y)
    , Z(InVector.Z)
    , W(1.0f)
{
}

template<typename T>
inline FVector4<T>::FVector4(T InX, T InY, T InZ, T InW)
    : X(InX), Y(InY), Z(InZ), W(InW)
{
}

// =========================================================
// 사칙 연산자 오버로딩
// =========================================================
template<typename T>
inline FVector4<T> FVector4<T>::operator+(const FVector4<T>& V) const
{
    return FVector4<T>(X + V.X, Y + V.Y, Z + V.Z, W + V.W);
}

template<typename T>
inline FVector4<T> FVector4<T>::operator-(const FVector4<T>& V) const
{
    return FVector4(X - V.X, Y - V.Y, Z - V.Z, W - V.W);
}

template<typename T>
inline FVector4<T> FVector4<T>::operator*(T Scale) const
{
    return FVector4<T>(X * Scale, Y * Scale, Z * Scale, W * Scale); }

template <typename T> 
inline FVector4<T> FVector4<T>::operator*(const FMatrix<T>& M) const 
{ 
    return FVector4<T>(
        X * M.M[0][0] + Y * M.M[1][0] + Z * M.M[2][0] + W * M.M[3][0], 
        X * M.M[0][1] + Y * M.M[1][1] + Z * M.M[2][1] + W * M.M[3][1],
        X * M.M[0][2] + Y * M.M[1][2] + Z * M.M[2][2] + W * M.M[3][2], 
        X * M.M[0][3] + Y * M.M[1][3] + Z * M.M[2][3] + W * M.M[3][3]);
}

template<typename T>
inline FVector4<T> FVector4<T>::operator/(T Scale) const
{
    const T RScale = 1.0f / Scale;
    return FVector4<T>(X * RScale, Y * RScale, Z * RScale, W * RScale);
}

// =========================================================
// 복합 대입 연산자
// =========================================================
template<typename T>
inline FVector4<T>& FVector4<T>::operator+=(const FVector4<T>& V)
{
    X += V.X;
    Y += V.Y;
    Z += V.Z;
    W += V.W;
    return *this;
}

template<typename T>
inline FVector4<T>& FVector4<T>::operator-=(const FVector4<T>& V)
{
    X -= V.X;
    Y -= V.Y;
    Z -= V.Z;
    W -= V.W;
    return *this;
}

template <typename T> 
inline FVector4<T> &FVector4<T>::operator*=(T Scale)
{
    X *= Scale;
    Y *= Scale;
    Z *= Scale;
    W *= Scale;
    return *this;
}

template <typename T> 
inline FVector4<T> &FVector4<T>::operator/=(T Scale)
{
    const T InvScale = 1.0f / Scale;
    X *= InvScale;
    Y *= InvScale;
    Z *= InvScale;
    W *= InvScale;
    return *this;
}


// =========================================================
// 인스턴스 유틸리티 함수
// =========================================================
template<typename T>
inline T FVector4<T>::SizeSquared() const
{
    return X * X + Y * Y + Z * Z + W * W;
}

template<typename T>
inline T FVector4<T>::Size() const
{
    return FMath::Sqrt(SizeSquared());
}

template<typename T>
inline T FVector4<T>::Length() const
{
    return Size();
}

template<typename T>
inline void FVector4<T>::Normalize()
{
    T len = Size();
    if (len > 0.000001f)
    {
        X /= len;
        Y /= len;
        Z /= len;
        W /= len;
    }
}

// =========================================================
// 공용 수학 계산기 (Static)
// =========================================================
template<typename T>
inline T FVector4<T>::Dot4(const FVector4<T>& V1, const FVector4<T>& V2)
{
    return V1.X * V2.X + V1.Y * V2.Y + V1.Z * V2.Z + V1.W * V2.W;
}

template<typename T>
inline FVector4<T> FVector4<T>::operator^(const FVector4<T>& V) const
{
    return FVector4<T>(
        Y * V.Z - Z * V.Y,
        Z * V.X - X * V.Z,
        X * V.Y - Y * V.X,
        T(0.0f)
    );
}
