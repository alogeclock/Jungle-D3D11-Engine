#pragma once

#include "UnrealMathUtility.h"

template<typename T>
struct FVector
{
public:
    // ---------------------------------------------------------
    // 1. 멤버 변수 (Member Variables)
    // ---------------------------------------------------------
    T X;
    T Y;
    T Z;

    // ---------------------------------------------------------
    // 2. 생성자 (Constructors)
    // ---------------------------------------------------------
    FVector(T InX, T InY, T InZ);

    // ---------------------------------------------------------
    // 3. 일반 사칙 연산자 (Basic Math Operators)
    // ---------------------------------------------------------
    FVector<T> operator+(const FVector<T>& V) const;
    FVector<T> operator-(const FVector<T>& V) const;
    FVector<T> operator*(T Scale) const;
    FVector<T> operator/(T Scale) const;

    // ---------------------------------------------------------
    // 4. 복합 대입 연산자 (Assignment Operators)
    // ---------------------------------------------------------
    FVector<T>& operator+=(const FVector<T>& V);
    FVector<T>& operator-=(const FVector<T>& V); // +=이 있다면 -=도 짝꿍으로 필수!

    // ---------------------------------------------------------
    // 5. 인스턴스 유틸리티 함수 (Instance Utility Functions)
    // ---------------------------------------------------------
    T SizeSquared() const;
    T Size() const;
    T Length() const;
    bool Normalize(T Tolerance = 1.e-8f);

    // ---------------------------------------------------------
    // 6. 공용 수학 계산기 (Static Math Functions)
    // ---------------------------------------------------------
    static T DotProduct(const FVector<T>& A, const FVector<T>& B);
    static FVector<T> CrossProduct(const FVector<T>& A, const FVector<T>& B);
};

// =========================================================
// 생성자
// =========================================================
template<typename T>
inline FVector<T>::FVector(T InX, T InY, T InZ)
    : X(InX), Y(InY), Z(InZ)
{
}

// =========================================================
// 사칙 연산자 오버로딩
// =========================================================
template<typename T>
inline FVector<T> FVector<T>::operator+(const FVector& V) const
{
    return FVector(X + V.X, Y + V.Y, Z + V.Z);
}

template<typename T>
inline FVector<T> FVector<T>::operator-(const FVector& V) const
{
    return FVector<T>(X - V.X, Y - V.Y, Z - V.Z);
}

template<typename T>
inline FVector<T> FVector<T>::operator*(T Scale) const
{
    return FVector<T>(X * Scale, Y * Scale, Z * Scale);
}

template<typename T>
inline FVector<T> FVector<T>::operator/(T Scale) const
{
    const T RScale = 1.0f / Scale;
    return FVector<T>(X * RScale, Y * RScale, Z * RScale);
}

// =========================================================
// 복합 대입 연산자
// =========================================================
template<typename T>
inline FVector<T>& FVector<T>::operator+=(const FVector<T>& V)
{
    X += V.X;
    Y += V.Y;
    Z += V.Z;
    return *this;
}

template<typename T>
inline FVector<T>& FVector<T>::operator-=(const FVector<T>& V)
{
    X -= V.X;
    Y -= V.Y;
    Z -= V.Z;
    return *this;
}

// =========================================================
// 인스턴스 유틸리티 함수
// =========================================================
template<typename T>
inline T FVector<T>::SizeSquared() const
{
    return X * X + Y * Y + Z * Z;
}

template<typename T>
inline T FVector<T>::Size() const
{
    return FMath::Sqrt(X * X + Y * Y + Z * Z);
}

template<typename T>
inline T FVector<T>::Length() const
{
    return Size();
}

template<typename T>
inline bool FVector<T>::Normalize(T Tolerance)
{
    const T SquareSum = X * X + Y * Y + Z * Z;
    if (SquareSum > Tolerance)
    {
        const T Scale = FMath::InvSqrt(SquareSum);
        X *= Scale; Y *= Scale; Z *= Scale;
        return true;
    }
    return false;
}

// =========================================================
// 공용 수학 계산기 (Static)
// =========================================================
template<typename T>
inline T FVector<T>::DotProduct(const FVector<T>& A, const FVector<T>& B)
{
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}

template<typename T>
inline FVector<T> FVector<T>::CrossProduct(const FVector<T>& A, const FVector<T>& B)
{
    return FVector<T>(
        A.Y * B.Z - A.Z * B.Y,
        A.Z * B.X - A.X * B.Z,
        A.X * B.Y - A.Y * B.X
    );
}