#include "Mesh/Fbx/FbxTransformUtils.h"

#include <cmath>

// FBX 일반 matrix를 엔진 FMatrix로 변환한다.
FMatrix FFbxTransformUtils::ToEngineMatrix(const FbxMatrix& Source)
{
    FMatrix Result = FMatrix::Identity;

    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Col = 0; Col < 4; ++Col)
        {
            Result.M[Row][Col] = static_cast<float>(Source.Get(Row, Col));
        }
    }

    return Result;
}

// FBX affine matrix를 엔진 FMatrix로 변환한다.
FMatrix FFbxTransformUtils::ToEngineMatrix(const FbxAMatrix& Source)
{
    FMatrix Result = FMatrix::Identity;

    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Col = 0; Col < 4; ++Col)
        {
            Result.M[Row][Col] = static_cast<float>(Source.Get(Row, Col));
        }
    }

    return Result;
}

// node의 geometric transform을 FBX affine matrix로 구성한다.
FbxAMatrix FFbxTransformUtils::GetNodeGeometryTransform(FbxNode* Node)
{
    FbxAMatrix GeometryTransform;
    GeometryTransform.SetIdentity();

    if (!Node)
    {
        return GeometryTransform;
    }

    GeometryTransform.SetT(Node->GetGeometricTranslation(FbxNode::eSourcePivot));
    GeometryTransform.SetR(Node->GetGeometricRotation(FbxNode::eSourcePivot));
    GeometryTransform.SetS(Node->GetGeometricScaling(FbxNode::eSourcePivot));

    return GeometryTransform;
}

// 위치 벡터를 matrix로 변환한다.
FVector FFbxTransformUtils::TransformPositionByMatrix(const FVector& P, const FMatrix& M)
{
    return P * M;
}

// 방향 벡터를 matrix로 변환한 뒤 정규화한다.
FVector FFbxTransformUtils::TransformDirectionByMatrix(const FVector& V, const FMatrix& M)
{
    return M.TransformVector(V).Normalized();
}

// normal 벡터를 normal matrix로 변환한 뒤 정규화한다.
FVector FFbxTransformUtils::TransformNormalByMatrix(const FVector& V, const FMatrix& M)
{
    return M.TransformVector(V).Normalized();
}

// tangent를 normal에 직교하도록 보정한다.
FVector FFbxTransformUtils::OrthogonalizeTangentToNormal(const FVector& Tangent, const FVector& Normal)
{
    const FVector N = Normal.Normalized();

    FVector T = Tangent - (N * Tangent.Dot(N));

    if (T.IsNearlyZero(1.0e-6f))
    {
        const FVector Candidate = (std::fabs(N.Z) < 0.999f) ? FVector::UpVector : FVector::RightVector;

        T = Candidate - (N * Candidate.Dot(N));
    }

    return T.Normalized();
}

// tangent를 matrix로 변환하고 reference normal 기준으로 직교화한다.
FVector FFbxTransformUtils::TransformTangentByMatrix(const FVector& Tangent, const FMatrix& TangentMatrix, const FVector& ReferenceNormal)
{
    const FVector ReferenceTangent = TransformDirectionByMatrix(Tangent, TangentMatrix);
    return OrthogonalizeTangentToNormal(ReferenceTangent, ReferenceNormal);
}

// 벡터를 matrix로 변환하되 정규화하지 않는다.
FVector FFbxTransformUtils::TransformVectorNoNormalizeByMatrix(const FVector& V, const FMatrix& M)
{
    return M.TransformVector(V);
}

// matrix의 회전축을 정규화해 scale 성분을 제거한다.
FMatrix FFbxTransformUtils::RemoveScaleFromMatrix(const FMatrix& Matrix)
{
    FVector XAxis(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
    FVector YAxis(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
    FVector ZAxis(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

    XAxis = XAxis.Normalized();
    YAxis = YAxis.Normalized();
    ZAxis = ZAxis.Normalized();

    FMatrix Result = FMatrix::Identity;

    Result.M[0][0] = XAxis.X;
    Result.M[0][1] = XAxis.Y;
    Result.M[0][2] = XAxis.Z;

    Result.M[1][0] = YAxis.X;
    Result.M[1][1] = YAxis.Y;
    Result.M[1][2] = YAxis.Z;

    Result.M[2][0] = ZAxis.X;
    Result.M[2][1] = ZAxis.Y;
    Result.M[2][2] = ZAxis.Z;

    return Result;
}

// 엔진 local matrix를 animation key의 TRS 값으로 분해한다.
FBoneTransformKey FFbxTransformUtils::MakeBoneTransformKeyFromEngineMatrix(float TimeSeconds, const FMatrix& LocalMatrix)
{
    FBoneTransformKey Key;
    Key.TimeSeconds = TimeSeconds;
    Key.Translation = LocalMatrix.GetLocation();
    Key.Scale       = LocalMatrix.GetScale();
    Key.Rotation    = RemoveScaleFromMatrix(LocalMatrix).ToQuat().GetNormalized();
    return Key;
}
