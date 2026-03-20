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
};
