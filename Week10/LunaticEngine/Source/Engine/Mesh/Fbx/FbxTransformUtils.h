#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Mesh/SkeletalMeshAsset.h"

#include <fbxsdk.h>

class FFbxTransformUtils
{
public:
    // FBX 일반 matrix를 엔진 FMatrix로 변환한다.
    static FMatrix ToEngineMatrix(const FbxMatrix& Source);

    // FBX affine matrix를 엔진 FMatrix로 변환한다.
    static FMatrix ToEngineMatrix(const FbxAMatrix& Source);

    // node의 geometric transform을 FBX affine matrix로 구성한다.
    static FbxAMatrix GetNodeGeometryTransform(FbxNode* Node);

    // 위치 벡터를 matrix로 변환한다.
    static FVector TransformPositionByMatrix(const FVector& P, const FMatrix& M);

    // 방향 벡터를 matrix로 변환한 뒤 정규화한다.
    static FVector TransformDirectionByMatrix(const FVector& V, const FMatrix& M);

    // normal 벡터를 normal matrix로 변환한 뒤 정규화한다.
    static FVector TransformNormalByMatrix(const FVector& V, const FMatrix& M);

    // tangent를 normal에 직교하도록 보정한다.
    static FVector OrthogonalizeTangentToNormal(const FVector& Tangent, const FVector& Normal);

    // tangent를 matrix로 변환하고 reference normal 기준으로 직교화한다.
    static FVector TransformTangentByMatrix(const FVector& Tangent, const FMatrix& TangentMatrix, const FVector& ReferenceNormal);

    // 벡터를 matrix로 변환하되 정규화하지 않는다.
    static FVector TransformVectorNoNormalizeByMatrix(const FVector& V, const FMatrix& M);

    // matrix의 회전축을 정규화해 scale 성분을 제거한다.
    static FMatrix RemoveScaleFromMatrix(const FMatrix& Matrix);

    // 엔진 local matrix를 animation key의 TRS 값으로 분해한다.
    static FBoneTransformKey MakeBoneTransformKeyFromEngineMatrix(float TimeSeconds, const FMatrix& LocalMatrix);
};
