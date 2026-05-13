#include "Mesh/Fbx/FbxImportValidation.h"

#include <algorithm>
#include <cmath>

float FFbxImportValidation::ValidateBindPoseSkinningError(const FSkeletalMeshLOD& LOD, const FSkeleton& Skeleton)
{
    if (LOD.Vertices.empty() || Skeleton.Bones.empty())
    {
        return 0.0f;
    }

    float MaxError = 0.0f;
    const int32 BoneCount = static_cast<int32>(Skeleton.Bones.size());

    for (const FSkeletalVertex& Src : LOD.Vertices)
    {
        FVector SkinnedPos(0.0f, 0.0f, 0.0f);
        float TotalWeight = 0.0f;

        for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
        {
            const int32 BoneIndex = static_cast<int32>(Src.BoneIndices[InfluenceIndex]);
            const float  Weight    = Src.BoneWeights[InfluenceIndex];

            if (BoneIndex < 0 || BoneIndex >= BoneCount || !std::isfinite(Weight) || Weight <= 0.0f)
            {
                continue;
            }

            const FBoneInfo& Bone = Skeleton.Bones[BoneIndex];
            const FMatrix SkinningMatrix = Bone.InverseBindPose * Bone.GlobalBindPose;
            const FVector WeightedPos = (Src.Pos * SkinningMatrix) * Weight;
            if (!std::isfinite(WeightedPos.X) || !std::isfinite(WeightedPos.Y) || !std::isfinite(WeightedPos.Z))
            {
                continue;
            }

            SkinnedPos += WeightedPos;
            TotalWeight += Weight;
        }

        if (TotalWeight <= 1e-6f)
        {
            continue;
        }

        const float Error = (SkinnedPos - Src.Pos).Length();
        if (!std::isfinite(Error))
        {
            continue;
        }

        MaxError          = (std::max)(MaxError, Error);
    }

    return MaxError;
}
