#pragma once

#include "Plane.h"
#include "Matrix.h"

// ─────────────────────────────────────────
// 카메라 좌표계로 변환하는 행렬
// 
// Eye: 카메라의 월드 좌표 위치
// Target: 카메라가 바라보는 지점
// Up: 카메라의 위쪽 방향 (Z or Y)
// ─────────────────────────────────────────

template<typename T>
struct FViewMatrix : public FMatrix<T>
{
    FViewMatrix(const FVector<T>& Eye, const FVector<T>& Target, const FVector<T>& Up)
        : FMatrix<T>(Compute(Eye, Target, Up))
    { }

private:
    static FMatrix<T> Compute(const FVector<T>& Eye, const FVector<T>& Target, const FVector<T>& Up)
    {
        // 왼손 좌표계 (Forward = X, Right = Y, Up = Z)
        FVector Forward = (Target - Eye);
        Forward.Normalize();
        FVector Right = FVector<float>::CrossProduct(Up, Forward);
        Right.Normalize();
        FVector NewUp = FVector<float>::CrossProduct(Forward, Right);

        // Rotation의 역행렬(= 전치) + Translation의 역행렬(= 부호 반전)
        return FMatrix<T>(
            FPlane<T>(Right.X, NewUp.X, Forward.X, 0.0f),
            FPlane<T>(Right.Y, NewUp.Y, Forward.Y, 0.0f),
            FPlane<T>(Right.Z, NewUp.Z, Forward.Z, 0.0f),
            FPlane<T>(-(FVector<float>::DotProduct(Right, Eye)), -(FVector<float>::DotProduct(NewUp, Eye)), -(FVector<float>::DotProduct(Forward, Eye)), 1.0f)
        );
    }
};

//template<typename T>
//FViewMatrix<T>::FViewMatrix(const FVector<T>& Eye, const FVector<T>& Target, const FVector<T>& Up)
//    : FMatrix<float>()
//{
//    // 왼손 좌표계 (Forward = X, Right = Y, Up = Z)
//    FVector Forward = (Target - Eye);
//    Forward.Normalize();
//
//    FVector Right = FVector<float>::CrossProduct(Up, Forward);
//    Right.Normalize();
//
//    FVector NewUp = FVector<float>::CrossProduct(Forward, Right);
//
//    // Rotation의 역행렬(= 전치) + Translation의 역행렬(= 부호 반전)
//    *this = FMatrix<T>(
//        FPlane<T>(Right.X, NewUp.X, Forward.X, 0.0f),
//        FPlane<T>(Right.Y, NewUp.Y, Forward.Y, 0.0f),
//        FPlane<T>(Right.Z, NewUp.Z, Forward.Z, 0.0f),
//        FPlane<T>(-(FVector<float>::DotProduct(Right, Eye)), -(FVector<float>::DotProduct(NewUp, Eye)), -(FVector<float>::DotProduct(Forward, Eye)), 1.0f)
//    );
//};