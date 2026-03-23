#pragma once

//#include "Quat.h"
//#include "TransformNonVectorized.h"



#include "Source/Core/Public/Math/TranslationMatrix.h"
#include "Source/Core/Public/Math/ScaleMatrix.h"
#include "Source/Core/Public/Math/RotationMatrix.h"

struct FTransform
{
    FVector<float> Location;
    FVector<float> Rotation;
    FVector<float> Scale;

  public:
    FTransform() : Location(0.0f, 0.0f, 0.0f), Rotation(0.0f, 0.0f, 0.0f), Scale(1.0f, 1.0f, 1.0f)
    {
    }

    FTransform(FVector<float> InLocation, FVector<float> InRotation, FVector<float> InScale)
    {
        Location = InLocation;
        Rotation = InRotation;
        Scale = InScale;
    }

    // 트랜스폼 데이터를 기반으로 WorldMatrix를 생성하여 반환하는 함수
    FMatrix<float> ToMatrix() const
    {
        FMatrix<float> S = FScaleMatrix<float>(Scale);
        FMatrix<float> R = FRotationMatrix<float>(Rotation);
        FMatrix<float> T = FTranslationMatrix<float>(Location);

        return S * R * T;
    }

    FTransform ToTransform(FMatrix<float> M)
    {
        FTransform OutTransform;

    // 1. Location 추출 (행렬의 마지막 행/열 위치에 따라 인덱스 확인 필요)
    OutTransform.Location = FVector<float>(M.M[3][0], M.M[3][1], M.M[3][2]);

    // 2. Scale 추출
    FVector<float> BasisX(M.M[0][0], M.M[0][1], M.M[0][2]);
    FVector<float> BasisY(M.M[1][0], M.M[1][1], M.M[1][2]);
    FVector<float> BasisZ(M.M[2][0], M.M[2][1], M.M[2][2]);

    OutTransform.Scale.X = BasisX.Length();
    OutTransform.Scale.Y = BasisY.Length();
    OutTransform.Scale.Z = BasisZ.Length();

    // 3. Rotation 추출
    FMatrix<float> RotationMatrix = FMatrix<float>::Identity();

    if (OutTransform.Scale.X != 0.0f) BasisX = BasisX / OutTransform.Scale.X;
    if (OutTransform.Scale.Y != 0.0f) BasisY = BasisY / OutTransform.Scale.Y;
    if (OutTransform.Scale.Z != 0.0f) BasisZ = BasisZ / OutTransform.Scale.Z;

    RotationMatrix.M[0][0] = BasisX.X; RotationMatrix.M[0][1] = BasisX.Y; RotationMatrix.M[0][2] = BasisX.Z;
    RotationMatrix.M[1][0] = BasisY.X; RotationMatrix.M[1][1] = BasisY.Y; RotationMatrix.M[1][2] = BasisY.Z;
    RotationMatrix.M[2][0] = BasisZ.X; RotationMatrix.M[2][1] = BasisZ.Y; RotationMatrix.M[2][2] = BasisZ.Z;

    // 이미 구현되어 있는 ToEuler() 함수를 사용하여 오일러 각으로 변환합니다.
    OutTransform.Rotation = RotationMatrix.ToEuler();

    return OutTransform;
    }
};
