#pragma once

#include <cmath>

struct FVector4
{
public:
    // ---------------------------------------------------------
    // 1. 멤버 변수 (Member Variables)
    // ---------------------------------------------------------
    float X;
    float Y;
    float Z;
    float W;

    // ---------------------------------------------------------
    // 2. 생성자 (Constructors)
    // ---------------------------------------------------------
    FVector4();
    FVector4(float InX, float InY, float InZ, float InW);

    // ---------------------------------------------------------
    // 3. 일반 사칙 연산자 (Basic Math Operators)
    // ---------------------------------------------------------
    FVector4 operator+(const FVector4& V) const;
    FVector4 operator-(const FVector4& V) const;
    FVector4 operator*(float Scale) const;
    FVector4 operator/(float Scale) const;

    // ---------------------------------------------------------
    // 4. 복합 대입 연산자 (Assignment Operators)
    // ---------------------------------------------------------
    FVector4& operator+=(const FVector4& V);
    FVector4& operator-=(const FVector4& V); // +=이 있다면 -=도 짝꿍으로 필수!

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
    static float DotProduct(const FVector4& A, const FVector4& B);
    static FVector4 CrossProduct(const FVector4& A, const FVector4& B);
};

// =========================================================
// 생성자
// =========================================================
FVector4::FVector4()
    : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f)
{
}

FVector4::FVector4(float InX, float InY, float InZ, float InW)
    : X(InX), Y(InY), Z(InZ), W(InW)
{
}

// =========================================================
// 사칙 연산자 오버로딩
// =========================================================
FVector4 FVector4::operator+(const FVector4& V) const
{
    return FVector4(X + V.X, Y + V.Y, Z + V.Z, W + V.W);
}

FVector4 FVector4::operator-(const FVector4& V) const
{
    return FVector4(X - V.X, Y - V.Y, Z - V.Z, W - V.W);
}

FVector4 FVector4::operator*(float Scale) const
{
    return FVector4(X * Scale, Y * Scale, Z * Scale, W * Scale);
}

FVector4 FVector4::operator/(float Scale) const
{
    const float RScale = 1.0f / Scale;
    return FVector4(X * RScale, Y * RScale, Z * RScale, W * RScale);
}

// =========================================================
// 복합 대입 연산자
// =========================================================
FVector4& FVector4::operator+=(const FVector4& V)
{
    X += V.X;
    Y += V.Y;
    Z += V.Z;
    W += V.W;
    return *this;
}

FVector4& FVector4::operator-=(const FVector4& V)
{
    X -= V.X;
    Y -= V.Y;
    Z -= V.Z;
    W -= V.W;
    return *this;
}

// =========================================================
// 인스턴스 유틸리티 함수
// =========================================================
float FVector4::SizeSquared() const
{
    return X * X + Y * Y + Z * Z + W * W;
}

float FVector4::Size() const
{
    return sqrt(SizeSquared());
}

float FVector4::Length() const
{
    return Size();
}

void FVector4::Normalize()
{
    float len = Size();
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
float FVector4::DotProduct(const FVector4& A, const FVector4& B)
{
    return A.X * B.X
        + A.Y * B.Y
        + A.Z * B.Z
        + A.W * B.W;
}

FVector4 FVector4::CrossProduct(const FVector4& A, const FVector4& B)
{
    // Vector3로 계산
    return FVector4(
        A.Y * B.Z - A.Z * B.Y,
        A.Z * B.X - A.X * B.Z,
        A.X * B.Y - A.Y * B.X,
        0.0f
    );
}
