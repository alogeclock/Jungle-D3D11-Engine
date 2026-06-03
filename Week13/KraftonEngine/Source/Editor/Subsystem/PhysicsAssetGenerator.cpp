#include "Editor/Subsystem/PhysicsAssetGenerator.h"

#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Mesh/MeshManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletonAsset.h"
#include "Object/FUObjectArray.h"
#include "Physics/Assets/PhysicsAsset.h"
#include "Physics/Assets/PhysicsBodySetup.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr float ChildAxisAlignmentThreshold = 0.6f;
const FVector PhysicsAssetCapsuleAxis = FVector::ForwardVector;
FString GLastPhysicsAssetCreateWarning;

FQuat MakeAxisAlignmentQuat(const FVector &FromAxis, const FVector &ToAxis)
{
    FVector SafeFromAxis = FromAxis;
    FVector SafeToAxis = ToAxis;
    if (SafeFromAxis.IsNearlyZero() || SafeToAxis.IsNearlyZero())
    {
        return FQuat::Identity;
    }

    SafeFromAxis.Normalize();
    SafeToAxis.Normalize();

    const float AxisDot = std::clamp(SafeFromAxis.Dot(SafeToAxis), -1.0f, 1.0f);
    if (AxisDot >= 1.0f - 1.0e-4f)
    {
        return FQuat::Identity;
    }

    if (AxisDot <= -1.0f + 1.0e-4f)
    {
        FVector RotationAxis = SafeFromAxis.Cross(FVector::ForwardVector);
        if (RotationAxis.IsNearlyZero())
        {
            RotationAxis = SafeFromAxis.Cross(FVector::RightVector);
        }
        if (RotationAxis.IsNearlyZero())
        {
            RotationAxis = SafeFromAxis.Cross(FVector::UpVector);
        }

        RotationAxis.Normalize();
        return FQuat::FromAxisAngle(RotationAxis, Pi);
    }

    const FVector RotationAxis = SafeFromAxis.Cross(SafeToAxis);
    FQuat Result(RotationAxis.X, RotationAxis.Y, RotationAxis.Z, 1.0f + AxisDot);
    Result.Normalize();
    return Result;
}

FRotator MakeRotatorFromToAxes(const FVector &FromAxis, const FVector &ToAxis)
{
    return MakeAxisAlignmentQuat(FromAxis, ToAxis).ToRotator();
}

void ResetPhysicsAsset(UPhysicsAsset *PhysicsAsset)
{
    if (!PhysicsAsset)
    {
        return;
    }

    for (UPhysicsBodySetup *BodySetup : PhysicsAsset->GetMutableBodySetups())
    {
        if (BodySetup)
        {
            GUObjectArray.DestroyObject(BodySetup);
        }
    }

    PhysicsAsset->GetMutableBodySetups().clear();
    PhysicsAsset->GetMutableConstraintSetups().clear();
}

int32 CollectChildBoneLocalOffsets(const FSkeletonAsset *SkeletonAsset, int32 BoneIndex,
                                   TArray<FVector> &OutChildLocalOffsets)
{
    if (!SkeletonAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
    {
        return 0;
    }

    OutChildLocalOffsets.clear();
    for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(SkeletonAsset->Bones.size()); ++CandidateIndex)
    {
        const FBone &CandidateBone = SkeletonAsset->Bones[CandidateIndex];
        if (CandidateBone.ParentIndex != BoneIndex)
        {
            continue;
        }

        OutChildLocalOffsets.push_back(CandidateBone.LocalMatrix.GetLocation());
    }

    return static_cast<int32>(OutChildLocalOffsets.size());
}

bool TryGetRepresentativeChildAxis(const TArray<FVector> &ChildOffsets, FVector &OutAxis, float &OutLength)
{
    FVector WeightedDirectionSum = FVector::ZeroVector;
    FVector FarthestChildOffset = FVector::ZeroVector;
    float FarthestChildLength = 0.0f;
    TArray<FVector> ValidChildOffsets;

    for (const FVector &ChildOffset : ChildOffsets)
    {
        if (ChildOffset.IsNearlyZero())
        {
            continue;
        }

        const float ChildLength = ChildOffset.Length();
        WeightedDirectionSum += ChildOffset.Normalized() * ChildLength;
        ValidChildOffsets.push_back(ChildOffset);

        if (ChildLength > FarthestChildLength)
        {
            FarthestChildLength = ChildLength;
            FarthestChildOffset = ChildOffset;
        }
    }

    if (ValidChildOffsets.empty() || FarthestChildOffset.IsNearlyZero())
    {
        return false;
    }

    FVector RepresentativeAxis = FarthestChildOffset.Normalized();
    bool bChildrenMostlyAligned = false;
    if (!WeightedDirectionSum.IsNearlyZero())
    {
        const FVector BlendedAxis = WeightedDirectionSum.Normalized();
        bChildrenMostlyAligned = true;
        for (const FVector &ChildOffset : ValidChildOffsets)
        {
            if (ChildOffset.Normalized().Dot(BlendedAxis) < ChildAxisAlignmentThreshold)
            {
                bChildrenMostlyAligned = false;
                break;
            }
        }

        if (bChildrenMostlyAligned)
        {
            RepresentativeAxis = BlendedAxis;
        }
    }

    if (bChildrenMostlyAligned)
    {
        float MaxProjectedLength = 0.0f;
        for (const FVector &ChildOffset : ValidChildOffsets)
        {
            MaxProjectedLength = (std::max)(MaxProjectedLength, ChildOffset.Dot(RepresentativeAxis));
        }
        OutLength = (std::max)(MaxProjectedLength, FarthestChildLength * 0.5f);
    }
    else
    {
        OutLength = FarthestChildLength;
    }

    OutAxis = RepresentativeAxis;
    return OutLength > 0.0f;
}

bool TryGetBodyAxisAndLength(const FSkeletonAsset *SkeletonAsset, int32 BoneIndex, FVector &OutAxis, float &OutLength)
{
    if (!SkeletonAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
    {
        return false;
    }

    FVector LocalAxis = FVector::ForwardVector;
    TArray<FVector> ChildOffsets;
    if (CollectChildBoneLocalOffsets(SkeletonAsset, BoneIndex, ChildOffsets) > 0 &&
        TryGetRepresentativeChildAxis(ChildOffsets, OutAxis, OutLength))
    {
        return true;
    }

    const FBone &Bone = SkeletonAsset->Bones[BoneIndex];
    if (Bone.ParentIndex >= 0)
    {
        const FVector ParentOriginInBoneLocal =
            Bone.LocalMatrix.GetInverse().TransformPositionWithW(FVector::ZeroVector);
        if (!ParentOriginInBoneLocal.IsNearlyZero())
        {
            // Leaf bodies should still be authored in the current bone's local frame.
            // Converting the parent origin through the inverse local matrix preserves the
            // bone's own basis instead of assuming parent-space axes line up.
            OutAxis = ParentOriginInBoneLocal.Normalized();
            OutLength = ParentOriginInBoneLocal.Length();
            return true;
        }
    }

    OutAxis = LocalAxis;
    OutLength = 0.0f;
    return false;
}

FPhysicsCollisionDesc MakeDefaultCollisionDesc()
{
    FPhysicsCollisionDesc CollisionDesc;
    CollisionDesc.CollisionEnabled = EPhysicsCollisionEnabled::PCE_QueryAndPhysics;
    CollisionDesc.ObjectChannel = EPhysicsCollisionChannel::PCC_PhysicsBody;
    return CollisionDesc;
}

void AddShapeToBody(UPhysicsBodySetup *BodySetup, const FName &BoneName, const FVector &LocalAxis, float BoneLength,
                    const FPhysicsAssetCreateParams &CreateParams)
{
    if (!BodySetup)
    {
        return;
    }

    const float SafeBoneLength = (std::max)(BoneLength, CreateParams.MinBoneSize * 0.5f);
    const float Radius = (std::max)(4.0f, SafeBoneLength * 0.2f);
    const FVector Center = LocalAxis * (SafeBoneLength * 0.5f);
    const FPhysicsCollisionDesc CollisionDesc = MakeDefaultCollisionDesc();

    FPhysicsAggregateShapeSetup &ShapeSetup = BodySetup->GetMutableShapeSetup();
    switch (CreateParams.PrimitiveType)
    {
    case EPhysicsAssetPrimitiveType::Sphere: {
        FPhysicsSphereShapeSetup SphereShape;
        SphereShape.Name = FName(BoneName.ToString() + "_Sphere");
        SphereShape.LocalTransform.Location = Center;
        SphereShape.Radius = (std::max)(Radius, SafeBoneLength * 0.35f);
        SphereShape.CollisionDesc = CollisionDesc;
        ShapeSetup.SphereShapeSetups.push_back(SphereShape);
        break;
    }

    case EPhysicsAssetPrimitiveType::Box: {
        FPhysicsBoxShapeSetup BoxShape;
        BoxShape.Name = FName(BoneName.ToString() + "_Box");
        BoxShape.LocalTransform.Location = Center;
        BoxShape.LocalTransform.SetRotation(CreateParams.bAutoOrientToBone
                                                ? MakeRotatorFromToAxes(FVector::ForwardVector, LocalAxis)
                                                : FRotator::ZeroRotator);
        BoxShape.HalfExtent = FVector(SafeBoneLength * 0.5f, Radius, Radius);
        BoxShape.CollisionDesc = CollisionDesc;
        ShapeSetup.BoxShapeSetups.push_back(BoxShape);
        break;
    }

    case EPhysicsAssetPrimitiveType::Capsule:
    default: {
        FPhysicsCapsuleShapeSetup CapsuleShape;
        CapsuleShape.Name = FName(BoneName.ToString() + "_Capsule");
        CapsuleShape.LocalTransform.Location = Center;
        CapsuleShape.LocalTransform.SetRotation(CreateParams.bAutoOrientToBone
                                                    ? MakeRotatorFromToAxes(PhysicsAssetCapsuleAxis, LocalAxis)
                                                    : FRotator::ZeroRotator);
        CapsuleShape.Radius = Radius;
        CapsuleShape.Length = (std::max)(SafeBoneLength - Radius * 2.0f, 0.0f);
        CapsuleShape.CollisionDesc = CollisionDesc;
        ShapeSetup.CapsuleShapeSetups.push_back(CapsuleShape);
        break;
    }
    }
}

struct FBoneShapeFit
{
    bool bHasVertices = false;
    FVector Min = FVector::ZeroVector;
    FVector Max = FVector::ZeroVector;
    TArray<FVector> Positions;

    void AddPosition(const FVector &Position)
    {
        Positions.push_back(Position);
        if (!bHasVertices)
        {
            Min = Position;
            Max = Position;
            bHasVertices = true;
            return;
        }

        Min.X = (std::min)(Min.X, Position.X);
        Min.Y = (std::min)(Min.Y, Position.Y);
        Min.Z = (std::min)(Min.Z, Position.Z);
        Max.X = (std::max)(Max.X, Position.X);
        Max.Y = (std::max)(Max.Y, Position.Y);
        Max.Z = (std::max)(Max.Z, Position.Z);
    }

    FVector GetCenter() const
    {
        return (Min + Max) * 0.5f;
    }
    FVector GetExtent() const
    {
        return (Max - Min) * 0.5f;
    }
};

struct FShapeFitFrame
{
    FTransform ElementTransform;
    FVector BoxExtent = FVector::ZeroVector;
};

int32 GetDominantBoneIndex(const FVertexPNCTBW &Vertex, float &OutWeight)
{
    int32 BestBoneIndex = -1;
    OutWeight = 0.0f;

    for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
    {
        const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
        const float Weight = Vertex.BoneWeights[InfluenceIndex];
        if (BoneIndex >= 0 && Weight > OutWeight)
        {
            BestBoneIndex = BoneIndex;
            OutWeight = Weight;
        }
    }

    return BestBoneIndex;
}

TArray<FBoneShapeFit> BuildBoneShapeFits(const FSkeletalMesh *MeshAsset, const FSkeletonAsset *SkeletonAsset,
                                         const FPhysicsAssetCreateParams &CreateParams)
{
    TArray<FBoneShapeFit> Fits;
    const int32 BoneCount = SkeletonAsset ? static_cast<int32>(SkeletonAsset->Bones.size()) : 0;
    Fits.resize(BoneCount);

    if (!MeshAsset || !SkeletonAsset || MeshAsset->Vertices.empty() || BoneCount <= 0)
    {
        return Fits;
    }

    for (const FVertexPNCTBW &Vertex : MeshAsset->Vertices)
    {
        if (CreateParams.VertexWeightingType == EPhysicsAssetVertexWeightingType::DominantWeight)
        {
            float BestWeight = 0.0f;
            const int32 BoneIndex = GetDominantBoneIndex(Vertex, BestWeight);
            if (BoneIndex >= 0 && BoneIndex < BoneCount)
            {
                const FVector LocalPosition =
                    SkeletonAsset->Bones[BoneIndex].InverseBindPoseMatrix.TransformPositionWithW(Vertex.Position);
                Fits[BoneIndex].AddPosition(LocalPosition);
            }
            continue;
        }

        for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
        {
            const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
            const float Weight = Vertex.BoneWeights[InfluenceIndex];
            if (BoneIndex >= 0 && BoneIndex < BoneCount && Weight > CreateParams.MinVertexWeight)
            {
                const FVector LocalPosition =
                    SkeletonAsset->Bones[BoneIndex].InverseBindPoseMatrix.TransformPositionWithW(Vertex.Position);
                Fits[BoneIndex].AddPosition(LocalPosition);
            }
        }
    }

    return Fits;
}

float GetBoneFitSize(const FBoneShapeFit &Fit)
{
    return Fit.bHasVertices ? Fit.GetExtent().Length() : 0.0f;
}

bool IsValidBoneIndex(const FSkeletonAsset *SkeletonAsset, int32 BoneIndex)
{
    return SkeletonAsset && BoneIndex >= 0 && BoneIndex < static_cast<int32>(SkeletonAsset->Bones.size());
}

bool IsFitUsable(const FBoneShapeFit &Fit, const FPhysicsAssetCreateParams &CreateParams)
{
    return GetBoneFitSize(Fit) > CreateParams.MinBoneSize;
}

FBoneShapeFit TransformFitToBoneSpace(const FSkeletonAsset *SkeletonAsset, const FBoneShapeFit &SourceFit,
                                      int32 SourceBoneIndex, int32 TargetBoneIndex)
{
    FBoneShapeFit Result;
    if (!IsValidBoneIndex(SkeletonAsset, SourceBoneIndex) || !IsValidBoneIndex(SkeletonAsset, TargetBoneIndex) ||
        !SourceFit.bHasVertices)
    {
        return Result;
    }

    if (SourceBoneIndex == TargetBoneIndex)
    {
        return SourceFit;
    }

    const FMatrix SourceToTarget = SkeletonAsset->Bones[SourceBoneIndex].GlobalMatrix *
                                   SkeletonAsset->Bones[TargetBoneIndex].GlobalMatrix.GetInverse();

    for (const FVector &Position : SourceFit.Positions)
    {
        Result.AddPosition(SourceToTarget.TransformPositionWithW(Position));
    }

    return Result;
}

void AppendFit(FBoneShapeFit &TargetFit, const FBoneShapeFit &SourceFit)
{
    if (!SourceFit.bHasVertices)
    {
        return;
    }

    for (const FVector &Position : SourceFit.Positions)
    {
        TargetFit.AddPosition(Position);
    }
}

void AppendFitToBoneSpace(const FSkeletonAsset *SkeletonAsset, FBoneShapeFit &TargetFit, const FBoneShapeFit &SourceFit,
                          int32 SourceBoneIndex, int32 TargetBoneIndex)
{
    AppendFit(TargetFit, TransformFitToBoneSpace(SkeletonAsset, SourceFit, SourceBoneIndex, TargetBoneIndex));
}

FBoneShapeFit MakeDefaultBoneFit()
{
    FBoneShapeFit Fit;
    Fit.AddPosition(FVector::ZeroVector);
    return Fit;
}

struct FMergedBoneFitData
{
    TArray<FBoneShapeFit> ExtraFits;
    TArray<float> MergedSizes;
    int32 ForcedRootBoneIndex = -1;
};

FMergedBoneFitData BuildMergedBoneFitData(const FSkeletonAsset *SkeletonAsset, const TArray<FBoneShapeFit> &BoneFits,
                                          const FPhysicsAssetCreateParams &CreateParams)
{
    FMergedBoneFitData Data;
    const int32 BoneCount = SkeletonAsset ? static_cast<int32>(SkeletonAsset->Bones.size()) : 0;
    Data.ExtraFits.resize(BoneCount);
    Data.MergedSizes.resize(BoneCount, 0.0f);

    for (int32 BoneIndex = BoneCount - 1; BoneIndex >= 0; --BoneIndex)
    {
        const float MyMergedSize =
            Data.MergedSizes[BoneIndex] +
            ((BoneIndex < static_cast<int32>(BoneFits.size())) ? GetBoneFitSize(BoneFits[BoneIndex]) : 0.0f);
        Data.MergedSizes[BoneIndex] = MyMergedSize;

        if (!CreateParams.bWalkPastSmallBones || CreateParams.bCreateBodyForAllBones)
        {
            continue;
        }

        if (MyMergedSize >= CreateParams.MinBoneSize || MyMergedSize < CreateParams.MinWeldSize)
        {
            continue;
        }

        const int32 ParentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
        if (!IsValidBoneIndex(SkeletonAsset, ParentIndex))
        {
            continue;
        }

        Data.MergedSizes[ParentIndex] += MyMergedSize;
        if (BoneIndex < static_cast<int32>(BoneFits.size()))
        {
            AppendFitToBoneSpace(SkeletonAsset, Data.ExtraFits[ParentIndex], BoneFits[BoneIndex], BoneIndex,
                                 ParentIndex);
        }
        AppendFitToBoneSpace(SkeletonAsset, Data.ExtraFits[ParentIndex], Data.ExtraFits[BoneIndex], BoneIndex,
                             ParentIndex);
    }

    int32 FirstParentBoneIndex = -1;
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        if (Data.MergedSizes[BoneIndex] <= CreateParams.MinBoneSize)
        {
            continue;
        }

        const int32 ParentBoneIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
        if (ParentBoneIndex == -1)
        {
            break;
        }

        if (FirstParentBoneIndex == -1)
        {
            FirstParentBoneIndex = ParentBoneIndex;
            continue;
        }

        if (ParentBoneIndex == FirstParentBoneIndex)
        {
            Data.ForcedRootBoneIndex = ParentBoneIndex;
            break;
        }
    }

    return Data;
}

FMatrix ComputeCovarianceMatrix(const TArray<FVector> &Positions)
{
    FMatrix Covariance;
    if (Positions.empty())
    {
        return FMatrix::Identity;
    }

    FVector Mean = FVector::ZeroVector;
    for (const FVector &Position : Positions)
    {
        Mean += Position;
    }
    Mean /= static_cast<float>(Positions.size());

    for (const FVector &Position : Positions)
    {
        const FVector Error = Position - Mean;
        Covariance.M[0][0] += Error.X * Error.X;
        Covariance.M[0][1] += Error.X * Error.Y;
        Covariance.M[0][2] += Error.X * Error.Z;
        Covariance.M[1][0] += Error.Y * Error.X;
        Covariance.M[1][1] += Error.Y * Error.Y;
        Covariance.M[1][2] += Error.Y * Error.Z;
        Covariance.M[2][0] += Error.Z * Error.X;
        Covariance.M[2][1] += Error.Z * Error.Y;
        Covariance.M[2][2] += Error.Z * Error.Z;
    }

    const float InvCount = 1.0f / static_cast<float>(Positions.size());
    for (int32 Row = 0; Row < 3; ++Row)
    {
        for (int32 Col = 0; Col < 3; ++Col)
        {
            Covariance.M[Row][Col] *= InvCount;
        }
    }

    Covariance.M[3][3] = 1.0f;
    return Covariance;
}

FVector ComputeDominantEigenVector(const FMatrix &Matrix)
{
    FVector Vector = FVector::ZAxisVector;
    for (int32 Iteration = 0; Iteration < 32; ++Iteration)
    {
        FVector NextVector = Matrix.TransformVector(Vector);
        const float Length = NextVector.Length();
        if (Length > 1.0e-6f)
        {
            Vector = NextVector / Length;
        }
    }

    if (Vector.IsNearlyZero())
    {
        return FVector::ZAxisVector;
    }
    Vector.Normalize();
    return Vector;
}

void FindBestAxisVectors(const FVector &AxisZ, FVector &AxisX, FVector &AxisY)
{
    FVector NormalizedZ = AxisZ;
    if (NormalizedZ.IsNearlyZero())
    {
        NormalizedZ = FVector::ZAxisVector;
    }
    else
    {
        NormalizedZ.Normalize();
    }

    const FVector UpVector = (std::fabs(NormalizedZ.Z) < 0.999f) ? FVector::ZAxisVector : FVector::XAxisVector;
    AxisX = UpVector.Cross(NormalizedZ);
    if (AxisX.IsNearlyZero())
    {
        AxisX = FVector::XAxisVector;
    }
    else
    {
        AxisX.Normalize();
    }

    AxisY = NormalizedZ.Cross(AxisX);
    if (AxisY.IsNearlyZero())
    {
        AxisY = FVector::YAxisVector;
    }
    else
    {
        AxisY.Normalize();
    }
}

FShapeFitFrame BuildShapeFitFrame(const FBoneShapeFit &Fit, const FPhysicsAssetCreateParams &CreateParams)
{
    FShapeFitFrame Frame;
    Frame.ElementTransform.Scale = FVector::OneVector;

    if (CreateParams.bAutoOrientToBone && Fit.Positions.size() >= 3)
    {
        const FMatrix Covariance = ComputeCovarianceMatrix(Fit.Positions);
        const FVector AxisZ = ComputeDominantEigenVector(Covariance);
        FVector AxisX;
        FVector AxisY;
        FindBestAxisVectors(AxisZ, AxisX, AxisY);

        FMatrix ElementMatrix = FMatrix::Identity;
        ElementMatrix.SetAxes(AxisX, AxisY, AxisZ);
        Frame.ElementTransform = FTransform(ElementMatrix);
        Frame.ElementTransform.Scale = FVector::OneVector;
    }

    bool bHasBounds = false;
    FVector LocalMin = FVector::ZeroVector;
    FVector LocalMax = FVector::ZeroVector;
    const FQuat InverseRotation = Frame.ElementTransform.Rotation.Inverse();

    for (const FVector &Position : Fit.Positions)
    {
        const FVector LocalPosition = InverseRotation.RotateVector(Position);
        if (!bHasBounds)
        {
            LocalMin = LocalPosition;
            LocalMax = LocalPosition;
            bHasBounds = true;
            continue;
        }

        LocalMin.X = (std::min)(LocalMin.X, LocalPosition.X);
        LocalMin.Y = (std::min)(LocalMin.Y, LocalPosition.Y);
        LocalMin.Z = (std::min)(LocalMin.Z, LocalPosition.Z);
        LocalMax.X = (std::max)(LocalMax.X, LocalPosition.X);
        LocalMax.Y = (std::max)(LocalMax.Y, LocalPosition.Y);
        LocalMax.Z = (std::max)(LocalMax.Z, LocalPosition.Z);
    }

    FVector BoxCenter = Fit.GetCenter();
    Frame.BoxExtent = Fit.GetExtent();
    if (bHasBounds)
    {
        BoxCenter = (LocalMin + LocalMax) * 0.5f;
        Frame.BoxExtent = (LocalMax - LocalMin) * 0.5f;
    }

    Frame.BoxExtent.X = (std::max)(Frame.BoxExtent.X, CreateParams.MinPrimitiveSize);
    Frame.BoxExtent.Y = (std::max)(Frame.BoxExtent.Y, CreateParams.MinPrimitiveSize);
    Frame.BoxExtent.Z = (std::max)(Frame.BoxExtent.Z, CreateParams.MinPrimitiveSize);
    Frame.ElementTransform.Location = Frame.ElementTransform.Rotation.RotateVector(BoxCenter);
    return Frame;
}

void AddFittedShapeToBody(UPhysicsBodySetup *BodySetup, const FName &BoneName, const FBoneShapeFit &Fit,
                          const FPhysicsAssetCreateParams &CreateParams)
{
    if (!BodySetup || !Fit.bHasVertices)
    {
        return;
    }

    const FShapeFitFrame FitFrame = BuildShapeFitFrame(Fit, CreateParams);
    const FTransform &ElementTransform = FitFrame.ElementTransform;
    const FVector BoxExtent = FitFrame.BoxExtent;
    const FPhysicsCollisionDesc CollisionDesc = MakeDefaultCollisionDesc();
    FPhysicsAggregateShapeSetup &ShapeSetup = BodySetup->GetMutableShapeSetup();

    switch (CreateParams.PrimitiveType)
    {
    case EPhysicsAssetPrimitiveType::Sphere: {
        FPhysicsSphereShapeSetup SphereShape;
        SphereShape.Name = FName(BoneName.ToString() + "_Sphere");
        SphereShape.LocalTransform.Location = ElementTransform.Location;
        SphereShape.Radius =
            (std::max)((std::max)(BoxExtent.X, (std::max)(BoxExtent.Y, BoxExtent.Z)) * CreateParams.FitPadding,
                       CreateParams.MinPrimitiveSize);
        SphereShape.CollisionDesc = CollisionDesc;
        ShapeSetup.SphereShapeSetups.push_back(SphereShape);
        break;
    }

    case EPhysicsAssetPrimitiveType::Box: {
        FPhysicsBoxShapeSetup BoxShape;
        BoxShape.Name = FName(BoneName.ToString() + "_Box");
        BoxShape.LocalTransform = ElementTransform;
        BoxShape.HalfExtent = BoxExtent * CreateParams.FitPadding;
        BoxShape.HalfExtent.X = (std::max)(BoxShape.HalfExtent.X, CreateParams.MinPrimitiveSize);
        BoxShape.HalfExtent.Y = (std::max)(BoxShape.HalfExtent.Y, CreateParams.MinPrimitiveSize);
        BoxShape.HalfExtent.Z = (std::max)(BoxShape.HalfExtent.Z, CreateParams.MinPrimitiveSize);
        BoxShape.CollisionDesc = CollisionDesc;
        ShapeSetup.BoxShapeSetups.push_back(BoxShape);
        break;
    }

    case EPhysicsAssetPrimitiveType::Capsule:
    default: {
        FPhysicsCapsuleShapeSetup CapsuleShape;
        CapsuleShape.Name = FName(BoneName.ToString() + "_Capsule");
        CapsuleShape.LocalTransform = ElementTransform;

        FQuat CapsuleRotation = ElementTransform.Rotation;
        if (BoxExtent.X > BoxExtent.Y && BoxExtent.X > BoxExtent.Z)
        {
            CapsuleShape.Radius = (std::max)((std::max)(BoxExtent.Y, BoxExtent.Z) * CreateParams.FitPadding,
                                             CreateParams.MinPrimitiveSize);
            CapsuleShape.Length =
                (std::max)(BoxExtent.X * 2.0f * CreateParams.FitPadding - CapsuleShape.Radius * 2.0f, 0.0f);
        }
        else if (BoxExtent.Y > BoxExtent.X && BoxExtent.Y > BoxExtent.Z)
        {
            const FVector TargetAxis = ElementTransform.Rotation.RotateVector(FVector::YAxisVector);
            CapsuleRotation = MakeAxisAlignmentQuat(PhysicsAssetCapsuleAxis, TargetAxis);
            CapsuleShape.Radius = (std::max)((std::max)(BoxExtent.X, BoxExtent.Z) * CreateParams.FitPadding,
                                             CreateParams.MinPrimitiveSize);
            CapsuleShape.Length =
                (std::max)(BoxExtent.Y * 2.0f * CreateParams.FitPadding - CapsuleShape.Radius * 2.0f, 0.0f);
        }
        else
        {
            const FVector TargetAxis = ElementTransform.Rotation.RotateVector(FVector::ZAxisVector);
            CapsuleRotation = MakeAxisAlignmentQuat(PhysicsAssetCapsuleAxis, TargetAxis);
            CapsuleShape.Radius = (std::max)((std::max)(BoxExtent.X, BoxExtent.Y) * CreateParams.FitPadding,
                                             CreateParams.MinPrimitiveSize);
            CapsuleShape.Length =
                (std::max)(BoxExtent.Z * 2.0f * CreateParams.FitPadding - CapsuleShape.Radius * 2.0f, 0.0f);
        }

        CapsuleShape.LocalTransform.Rotation = CapsuleRotation.GetNormalized();
        CapsuleShape.CollisionDesc = CollisionDesc;
        ShapeSetup.CapsuleShapeSetups.push_back(CapsuleShape);
        break;
    }
    }
}

EPhysicsJointType ToConstraintJointType(EPhysicsAssetAngularConstraintMode Mode)
{
    switch (Mode)
    {
    case EPhysicsAssetAngularConstraintMode::Locked:
        return EPhysicsJointType::PJT_Fixed;
    case EPhysicsAssetAngularConstraintMode::Free:
        return EPhysicsJointType::PJT_Spherical;
    case EPhysicsAssetAngularConstraintMode::Limited:
    default:
        return EPhysicsJointType::PJT_D6;
    }
}

FTransform MakeRelativeFrame(const FMatrix &ChildGlobalMatrix, const FMatrix &ParentGlobalMatrix)
{
    return FTransform(ChildGlobalMatrix * ParentGlobalMatrix.GetInverse());
}

int32 FindParentBodyBoneIndex(const FSkeletonAsset *SkeletonAsset,
                              const TMap<int32, UPhysicsBodySetup *> &BodyByBoneIndex, int32 BoneIndex,
                              bool bWalkPastSmallBones)
{
    if (!SkeletonAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
    {
        return -1;
    }

    int32 ParentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
    while (ParentIndex >= 0)
    {
        if (BodyByBoneIndex.find(ParentIndex) != BodyByBoneIndex.end())
        {
            return ParentIndex;
        }

        if (!bWalkPastSmallBones)
        {
            return -1;
        }

        ParentIndex = SkeletonAsset->Bones[ParentIndex].ParentIndex;
    }

    return -1;
}

bool ShouldCreateBoneBody(const FSkeletonAsset *SkeletonAsset, int32 BoneIndex,
                          const FPhysicsAssetCreateParams &CreateParams, bool bHasBoneSize, float BoneLength,
                          bool bHasUsableFit, const FBoneShapeFit *Fit)
{
    if (!SkeletonAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
    {
        return false;
    }

    if (CreateParams.bCreateBodyForAllBones)
    {
        return true;
    }

    if (bHasUsableFit && Fit)
    {
        return Fit->GetExtent().Length() >= (CreateParams.MinBoneSize * 0.5f);
    }

    const FBone &Bone = SkeletonAsset->Bones[BoneIndex];
    return (bHasBoneSize && BoneLength >= CreateParams.MinBoneSize) || (Bone.ParentIndex == -1 && bHasBoneSize);
}

bool PopulateVertexWeightPhysicsBodies(UPhysicsAsset *PhysicsAsset, const FSkeletalMesh *MeshAsset,
                                       const FSkeletonAsset *SkeletonAsset,
                                       const FPhysicsAssetCreateParams &CreateParams,
                                       TMap<int32, UPhysicsBodySetup *> &OutBodyByBoneIndex)
{
    if (!PhysicsAsset || !MeshAsset || !SkeletonAsset || MeshAsset->Vertices.empty() || SkeletonAsset->Bones.empty())
    {
        return false;
    }

    const TArray<FBoneShapeFit> BoneFits = BuildBoneShapeFits(MeshAsset, SkeletonAsset, CreateParams);
    const FMergedBoneFitData MergedBoneData = BuildMergedBoneFitData(SkeletonAsset, BoneFits, CreateParams);

    auto ShouldMakeBone = [&](int32 BoneIndex) -> bool {
        if (!IsValidBoneIndex(SkeletonAsset, BoneIndex))
        {
            return false;
        }

        if (CreateParams.bCreateBodyForAllBones)
        {
            return true;
        }

        if (BoneIndex < static_cast<int32>(MergedBoneData.MergedSizes.size()) &&
            MergedBoneData.MergedSizes[BoneIndex] > CreateParams.MinBoneSize)
        {
            return true;
        }

        return BoneIndex == MergedBoneData.ForcedRootBoneIndex;
    };

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(SkeletonAsset->Bones.size()); ++BoneIndex)
    {
        if (!ShouldMakeBone(BoneIndex))
        {
            continue;
        }

        FBoneShapeFit BodyFit;
        if (BoneIndex < static_cast<int32>(BoneFits.size()))
        {
            BodyFit = BoneFits[BoneIndex];
        }
        if (BoneIndex < static_cast<int32>(MergedBoneData.ExtraFits.size()))
        {
            AppendFit(BodyFit, MergedBoneData.ExtraFits[BoneIndex]);
        }

        if (!BodyFit.bHasVertices && CreateParams.bCreateBodyForAllBones)
        {
            BodyFit = MakeDefaultBoneFit();
        }

        if (!BodyFit.bHasVertices)
        {
            continue;
        }

        const FBone &Bone = SkeletonAsset->Bones[BoneIndex];
        UPhysicsBodySetup *BodySetup = GUObjectArray.CreateObject<UPhysicsBodySetup>(PhysicsAsset);
        BodySetup->SetTargetBoneName(FName(Bone.Name));
        BodySetup->SetBodyType(EPhysicsBodyType::PBT_Dynamic);
        BodySetup->SetMass((std::max)(0.1f, GetBoneFitSize(BodyFit) * 0.1f));
        BodySetup->SetLinearDamping(0.01f);
        BodySetup->SetAngularDamping(0.0f);
        BodySetup->SetCollisionDesc(MakeDefaultCollisionDesc());

        AddFittedShapeToBody(BodySetup, FName(Bone.Name), BodyFit, CreateParams);
        PhysicsAsset->AddBodySetup(BodySetup);
        OutBodyByBoneIndex.emplace(BoneIndex, BodySetup);
    }

    return !OutBodyByBoneIndex.empty();
}

void PopulateBoneLengthPhysicsBodies(UPhysicsAsset *PhysicsAsset, const FSkeletonAsset *SkeletonAsset,
                                     const FPhysicsAssetCreateParams &CreateParams,
                                     TMap<int32, UPhysicsBodySetup *> &OutBodyByBoneIndex)
{
    if (!PhysicsAsset || !SkeletonAsset)
    {
        return;
    }

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(SkeletonAsset->Bones.size()); ++BoneIndex)
    {
        const FBone &Bone = SkeletonAsset->Bones[BoneIndex];

        FVector LocalAxis = FVector::ForwardVector;
        float BoneLength = 0.0f;
        const bool bHasBoneSize = TryGetBodyAxisAndLength(SkeletonAsset, BoneIndex, LocalAxis, BoneLength);
        if (!ShouldCreateBoneBody(SkeletonAsset, BoneIndex, CreateParams, bHasBoneSize, BoneLength, false, nullptr))
        {
            continue;
        }

        UPhysicsBodySetup *BodySetup = GUObjectArray.CreateObject<UPhysicsBodySetup>(PhysicsAsset);
        BodySetup->SetTargetBoneName(FName(Bone.Name));
        BodySetup->SetBodyType(EPhysicsBodyType::PBT_Dynamic);
        BodySetup->SetMass((std::max)(0.1f, BoneLength * 0.05f));
        BodySetup->SetLinearDamping(0.01f);
        BodySetup->SetAngularDamping(0.0f);
        BodySetup->SetCollisionDesc(MakeDefaultCollisionDesc());

        AddShapeToBody(BodySetup, FName(Bone.Name), LocalAxis, BoneLength, CreateParams);
        PhysicsAsset->AddBodySetup(BodySetup);
        OutBodyByBoneIndex.emplace(BoneIndex, BodySetup);
    }
}

void BuildGeneratedPhysicsBodies(UPhysicsAsset *PhysicsAsset, const FSkeletalMesh *MeshAsset,
                                 const FSkeletonAsset *SkeletonAsset, const FPhysicsAssetCreateParams &CreateParams,
                                 TMap<int32, UPhysicsBodySetup *> &OutBodyByBoneIndex)
{
    if (!PhysicsAsset || !SkeletonAsset)
    {
        return;
    }

    if (CreateParams.BodyGenerationMethod == EPhysicsAssetBodyGenerationMethod::VertexWeight)
    {
        bool bPopulated =
            PopulateVertexWeightPhysicsBodies(PhysicsAsset, MeshAsset, SkeletonAsset, CreateParams, OutBodyByBoneIndex);
        if (!bPopulated)
        {
            FPhysicsAssetCreateParams RetryParams = CreateParams;
            RetryParams.MinBoneSize = 1.0f;
            bPopulated = PopulateVertexWeightPhysicsBodies(PhysicsAsset, MeshAsset, SkeletonAsset, RetryParams,
                                                           OutBodyByBoneIndex);
            if (bPopulated && GLastPhysicsAssetCreateWarning.empty())
            {
                GLastPhysicsAssetCreateWarning =
                    "Vertex-weight PhysicsAsset generation needed a relaxed MinBoneSize retry.";
            }
        }

        if (bPopulated)
        {
            return;
        }

        if (!CreateParams.bFallbackToBoneLength)
        {
            if (GLastPhysicsAssetCreateWarning.empty())
            {
                GLastPhysicsAssetCreateWarning =
                    "Vertex-weight PhysicsAsset generation failed and bone-length fallback is disabled.";
            }
            return;
        }

        GLastPhysicsAssetCreateWarning =
            "Vertex-weight PhysicsAsset generation could not create any body. Falling back to bone-length generation.";
    }

    PopulateBoneLengthPhysicsBodies(PhysicsAsset, SkeletonAsset, CreateParams, OutBodyByBoneIndex);
}

void BuildGeneratedPhysicsConstraints(UPhysicsAsset *PhysicsAsset, const FSkeletonAsset *SkeletonAsset,
                                      const TMap<int32, UPhysicsBodySetup *> &BodyByBoneIndex,
                                      const FPhysicsAssetCreateParams &CreateParams)
{
    if (!PhysicsAsset || !SkeletonAsset || !CreateParams.bCreateConstraints)
    {
        return;
    }

    for (const auto &Pair : BodyByBoneIndex)
    {
        const int32 BoneIndex = Pair.first;
        const int32 ParentBodyBoneIndex =
            FindParentBodyBoneIndex(SkeletonAsset, BodyByBoneIndex, BoneIndex, CreateParams.bWalkPastSmallBones);
        if (ParentBodyBoneIndex < 0)
        {
            continue;
        }

        const FBone &Bone = SkeletonAsset->Bones[BoneIndex];
        const FBone &ParentBone = SkeletonAsset->Bones[ParentBodyBoneIndex];

        FPhysicsConstraintSetup ConstraintSetup;
        ConstraintSetup.ConstraintName = FName(Bone.Name + "_Constraint");
        ConstraintSetup.ParentBoneName = FName(ParentBone.Name);
        ConstraintSetup.ChildBoneName = FName(Bone.Name);
        ConstraintSetup.JointType = ToConstraintJointType(CreateParams.AngularConstraintMode);
        ConstraintSetup.ParentLocalFrame = MakeRelativeFrame(Bone.GlobalMatrix, ParentBone.GlobalMatrix);
        ConstraintSetup.ChildLocalFrame = FTransform();
        ConstraintSetup.LinearLimit = 0.0f;
        ConstraintSetup.AngularLimit =
            CreateParams.AngularConstraintMode == EPhysicsAssetAngularConstraintMode::Locked ? 0.0f : 45.0f;
        ConstraintSetup.TwistLimitMin =
            CreateParams.AngularConstraintMode == EPhysicsAssetAngularConstraintMode::Free ? -180.0f : -45.0f;
        ConstraintSetup.TwistLimitMax =
            CreateParams.AngularConstraintMode == EPhysicsAssetAngularConstraintMode::Free ? 180.0f : 45.0f;
        ConstraintSetup.SwingLimitY =
            CreateParams.AngularConstraintMode == EPhysicsAssetAngularConstraintMode::Free ? 180.0f : 30.0f;
        ConstraintSetup.SwingLimitZ =
            CreateParams.AngularConstraintMode == EPhysicsAssetAngularConstraintMode::Free ? 180.0f : 30.0f;
        ConstraintSetup.bDisableCollision = CreateParams.bDisableCollisionsByDefault;

        PhysicsAsset->AddConstraintSetup(ConstraintSetup);
    }
}
} // namespace

bool FPhysicsAssetGenerator::PopulatePhysicsAssetFromSkeletalMesh(UPhysicsAsset *PhysicsAsset,
                                                                  const FString &SkeletalMeshPath,
                                                                  const FPhysicsAssetCreateParams &CreateParams,
                                                                  ID3D11Device *Device)
{
    GLastPhysicsAssetCreateWarning.clear();
    if (!PhysicsAsset || SkeletalMeshPath.empty())
    {
        return false;
    }

    USkeletalMesh *SkeletalMesh = FMeshManager::FindSkeletalMesh(SkeletalMeshPath);
    if (!SkeletalMesh)
    {
        if (!Device)
        {
            return false;
        }

        SkeletalMesh = FMeshManager::LoadSkeletalMesh(SkeletalMeshPath, Device);
    }

    if (!SkeletalMesh)
    {
        return false;
    }

    const FSkeletonAsset *SkeletonAsset = SkeletalMesh->GetSkeletonAsset();
    if (!SkeletonAsset)
    {
        return false;
    }

    ResetPhysicsAsset(PhysicsAsset);
    PhysicsAsset->SetPreviewSkeletalMeshPath(FPaths::MakeProjectRelative(SkeletalMeshPath));
    PhysicsAsset->SetRagdollMode(CreateParams.RagdollMode);

    TMap<int32, UPhysicsBodySetup *> BodyByBoneIndex;
    BuildGeneratedPhysicsBodies(PhysicsAsset, SkeletalMesh->GetSkeletalMeshAsset(), SkeletonAsset, CreateParams,
                                BodyByBoneIndex);
    BuildGeneratedPhysicsConstraints(PhysicsAsset, SkeletonAsset, BodyByBoneIndex, CreateParams);
    return true;
}

const FString &FPhysicsAssetGenerator::GetLastPhysicsAssetCreateWarning()
{
    return GLastPhysicsAssetCreateWarning;
}
