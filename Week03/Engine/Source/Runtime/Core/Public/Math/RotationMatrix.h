#pragma once

#include "Plane.h"
#include "Matrix.h"

// ─────────────────────────────────────────
// X축 회전 행렬 (Pitch)
// ─────────────────────────────────────────
template<typename T>
struct FRotationXMatrix : public FMatrix<T>
{
    FRotationXMatrix(T AngleRad);
};

template<typename T>
FRotationXMatrix<T>::FRotationXMatrix(T AngleRad)
    : FMatrix<T>(
        FPlane<T>(1.0f, 0.0f, 0.0f, 0.0f),
        FPlane<T>(0.0f, FMath::Cos(AngleRad), -FMath::Sin(AngleRad), 0.0f),
        FPlane<T>(0.0f, FMath::Sin(AngleRad), FMath::Cos(AngleRad), 0.0f),
        FPlane<T>(0.0f, 0.0f, 0.0f, 1.0f)
    )
{
}

// ─────────────────────────────────────────
// Y축 회전 행렬 (Yaw)
// ─────────────────────────────────────────
template<typename T>
struct FRotationYMatrix : public FMatrix<T>
{
    FRotationYMatrix(T AngleRad);
};

template<typename T>
FRotationYMatrix<T>::FRotationYMatrix(T AngleRad)
    : FMatrix<T>(
        FPlane<T>(FMath::Cos(AngleRad), 0.0f, FMath::Sin(AngleRad), 0.0f),
        FPlane<T>(0.0f, 1.0f, 0.0f, 0.0f),
        FPlane<T>(-FMath::Sin(AngleRad), 0.0f, FMath::Cos(AngleRad), 0.0f),
        FPlane<T>(0.0f, 0.0f, 0.0f, 1.0f)
    )
{
}

// ─────────────────────────────────────────
// Z축 회전 행렬 (Roll)
// ─────────────────────────────────────────
template<typename T>
struct FRotationZMatrix : public FMatrix<T>
{
    FRotationZMatrix(T AngleRad);
};

template<typename T>
FRotationZMatrix<T>::FRotationZMatrix(T AngleRad)
    : FMatrix<T>(
        FPlane<T>(FMath::Cos(AngleRad), -FMath::Sin(AngleRad), 0.0f, 0.0f),
        FPlane<T>(FMath::Sin(AngleRad), FMath::Cos(AngleRad), 0.0f, 0.0f),
        FPlane<T>(0.0f, 0.0f, 1.0f, 0.0f),
        FPlane<T>(0.0f, 0.0f, 0.0f, 1.0f)
    )
{
}

// ─────────────────────────────────────────
// 복합 회전 행렬 (Pitch / Yaw / Roll 동시 적용)
// 적용 순서: Rx(Pitch) * Ry(Yaw) * Rz(Roll)
// ─────────────────────────────────────────
template<typename T>
struct FRotationMatrix : public FMatrix<T>
{
    FRotationMatrix(T PitchRad, T YawRad, T RollRad);    
    FRotationMatrix(const FVector<T>& Rotation);
};

template<typename T>
FRotationMatrix<T>::FRotationMatrix(T PitchRad, T YawRad, T RollRad)
    : FMatrix<T>(
        FRotationXMatrix<T>(PitchRad)*
        FRotationYMatrix<T>(YawRad)*
        FRotationZMatrix<T>(RollRad)
    )
{
}

template<typename T>
FRotationMatrix<T>::FRotationMatrix(const FVector<T>& Rotation)
    : FMatrix<T>(
        FRotationXMatrix<T>(Rotation.X)*
        FRotationYMatrix<T>(Rotation.Y)*
        FRotationZMatrix<T>(Rotation.Z)
    )
{
}