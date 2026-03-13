#pragma once

#include "UnrealMathUtility.h"

struct FVector
{
public:
    // ---------------------------------------------------------
    // 1. 멤버 변수 (Member Variables)
    // ---------------------------------------------------------
    float X;
    float Y;
    float Z;

    // ---------------------------------------------------------
    // 2. 생성자 (Constructors)
    // ---------------------------------------------------------
    FVector();
    FVector(float InX, float InY, float InZ);

    // ---------------------------------------------------------
    // 3. 일반 사칙 연산자 (Basic Math Operators)
    // ---------------------------------------------------------
    FVector operator+(const FVector& V) const;
    FVector operator-(const FVector& V) const;
    FVector operator*(float Scale) const;
    FVector operator/(float Scale) const;

    // ---------------------------------------------------------
    // 4. 복합 대입 연산자 (Assignment Operators)
    // ---------------------------------------------------------
    FVector& operator+=(const FVector& V);
    FVector& operator-=(const FVector& V); // +=이 있다면 -=도 짝꿍으로 필수!

    // ---------------------------------------------------------
    // 5. 인스턴스 유틸리티 함수 (Instance Utility Functions)
    // ---------------------------------------------------------
    float SizeSquared() const;
    float Size() const;
    float Length() const;
    void Normalize();

    // ---------------------------------------------------------
    // 6. 공용 수학 계산기 (Static Math Functions)
    // ---------------------------------------------------------
    static float DotProduct(const FVector& A, const FVector& B);
    static FVector CrossProduct(const FVector& A, const FVector& B);
};

// =========================================================
// 생성자
// =========================================================
FVector::FVector()
    : X(0.0f), Y(0.0f), Z(0.0f)
{
}

FVector::FVector(float InX, float InY, float InZ)
    : X(InX), Y(InY), Z(InZ)
{
}

// =========================================================
// 사칙 연산자 오버로딩
// =========================================================
FVector FVector::operator+(const FVector& V) const
{
    return FVector(X + V.X, Y + V.Y, Z + V.Z);
}

FVector FVector::operator-(const FVector& V) const
{
    return FVector(X - V.X, Y - V.Y, Z - V.Z);
}

FVector FVector::operator*(float Scale) const
{
    return FVector(X * Scale, Y * Scale, Z * Scale);
}

FVector FVector::operator/(float Scale) const
{
    const float RScale = 1.0f / Scale;
    return FVector(X * RScale, Y * RScale, Z * RScale);
}

// =========================================================
// 복합 대입 연산자
// =========================================================
FVector& FVector::operator+=(const FVector& V)
{
    X += V.X;
    Y += V.Y;
    Z += V.Z;
    return *this;
}

FVector& FVector::operator-=(const FVector& V)
{
    X -= V.X;
    Y -= V.Y;
    Z -= V.Z;
    return *this;
}

// =========================================================
// 인스턴스 유틸리티 함수
// =========================================================
float FVector::SizeSquared() const
{
    return X * X + Y * Y + Z * Z;
}

float FVector::Size() const
{
    return FMath::Sqrt(X * X + Y * Y + Z * Z);
}

float FVector::Length() const
{
    return Size();
}

void FVector::Normalize() 
{
    float len = Size();
    if (len > 0.000001f)
    {
        X /= len;
        Y /= len;
        Z /= len;
    }
}

// =========================================================
// 공용 수학 계산기 (Static)
// =========================================================
float FVector::DotProduct(const FVector& A, const FVector& B)
{
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}

FVector FVector::CrossProduct(const FVector& A, const FVector& B)
{
    return FVector(
        A.Y * B.Z - A.Z * B.Y,
        A.Z * B.X - A.X * B.Z,
        A.X * B.Y - A.Y * B.X
    );
}