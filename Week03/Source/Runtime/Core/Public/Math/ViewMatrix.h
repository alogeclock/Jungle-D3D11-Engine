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
    FMatrix<T> Inverse() const;

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

template <typename T>
FMatrix<T> FViewMatrix<T>::Inverse() const
{
    // 원본 View Matrix : 
    // [Rx Ux Fx 0]
    // [Ry Uy Fy 0]
    // [Rz Uz Fz 0]
    // [-R·E - U·E - F·E 1]

    // 역행렬 = Camera → World 변환(= Camera Transform 그 자체)
    // [Rx Ry Rz 0]
    // [Ux Uy Uz 0]
    // [Fx Fy Fz 0]
    // [Ex Ey Ez 1]
    
    const float Rx = this->M[0][0], Ry = this->M[1][0], Rz = this->M[2][0];
    const float Ux = this->M[0][1], Uy = this->M[1][1], Uz = this->M[2][1];
    const float Fx = this->M[0][2], Fy = this->M[1][2], Fz = this->M[2][2];

    const float NegRdotE = this->M[3][0]; // -R·Eye
    const float NegUdotE = this->M[3][1]; // -U·Eye
    const float NegFdotE = this->M[3][2]; // -F·Eye

    // Eye 복원: E = R*(-R·E역) → 전치 행렬로 dot 역산
    const float Ex = -(Rx * NegRdotE + Ry * NegUdotE + Rz * NegFdotE);
    const float Ey = -(Ux * NegRdotE + Uy * NegUdotE + Uz * NegFdotE);
    const float Ez = -(Fx * NegRdotE + Fy * NegUdotE + Fz * NegFdotE);

    return FMatrix<T>(FPlane<T>(Rx, Ry, Rz, 0.f), // Right 행 → 열로 전치
                      FPlane<T>(Ux, Uy, Uz, 0.f), // Up
                      FPlane<T>(Fx, Fy, Fz, 0.f), // Forward
                      FPlane<T>(Ex, Ey, Ez, 1.f)  // Eye 위치 복원
    );
}
