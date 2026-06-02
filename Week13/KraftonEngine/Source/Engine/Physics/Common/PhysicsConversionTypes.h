#pragma once

#include "Object/FName.h"
#include "PhysicsDescTypes.h"

/**
 * @file PhysicsConversionTypes.h
 * @brief 엔진 타입과 Physics Runtime 타입 사이의 변환 기준 정의.
 */

/** Physics 단위 변환 기준 */
struct FPhysicsUnitScale
{
    static constexpr float LengthScale = 1.0f;
    static constexpr float MassScale   = 1.0f;
    static constexpr float TimeScale   = 1.0f;
};

inline FVector ScalePhysicsLength(const FVector& Value)
{
    return FVector(
        Value.X * FPhysicsUnitScale::LengthScale,
        Value.Y * FPhysicsUnitScale::LengthScale,
        Value.Z * FPhysicsUnitScale::LengthScale);
}

inline FTransform ScalePhysicsTransformLength(const FTransform& Transform)
{
    FTransform ScaledTransform = Transform;
    ScaledTransform.Location = ScalePhysicsLength(ScaledTransform.Location);
    return ScaledTransform;
}

inline FPhysicsShapeDesc MakeEngineUnitShapeDesc(FPhysicsShapeDesc ShapeDesc)
{
    ShapeDesc.LocalTransform = ScalePhysicsTransformLength(ShapeDesc.LocalTransform);
    ShapeDesc.Size = ScalePhysicsLength(ShapeDesc.Size);

    for (FVector& Vertex : ShapeDesc.VertexData)
    {
        Vertex = ScalePhysicsLength(Vertex);
    }

    return ShapeDesc;
}

inline FPhysicsBodyDesc MakeEngineUnitBodyDesc(FPhysicsBodyDesc BodyDesc)
{
    for (FPhysicsShapeDesc& ShapeDesc : BodyDesc.Shapes)
    {
        ShapeDesc = MakeEngineUnitShapeDesc(ShapeDesc);
    }

    return BodyDesc;
}

inline FPhysicsConstraintDesc MakeEngineUnitConstraintDesc(FPhysicsConstraintDesc ConstraintDesc)
{
    ConstraintDesc.ParentLocalFrame = ScalePhysicsTransformLength(ConstraintDesc.ParentLocalFrame);
    ConstraintDesc.ChildLocalFrame = ScalePhysicsTransformLength(ConstraintDesc.ChildLocalFrame);
    ConstraintDesc.LinearLimit *= FPhysicsUnitScale::LengthScale;
    return ConstraintDesc;
}

/** Bone Transform 기반 Body 생성 정보 */
struct FPhysicsTransformBuildInfo
{
    FName      BoneName           = FName::None;
    int32      BoneIndex          = INDEX_NONE;
    FTransform BoneWorldTransform;
    FTransform BoneLocalTransform;
};
