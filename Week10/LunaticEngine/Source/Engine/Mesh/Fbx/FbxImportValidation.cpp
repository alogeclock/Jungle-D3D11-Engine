#include "Mesh/Fbx/FbxImportValidation.h"

#include <algorithm>

float FFbxImportValidation::ValidateBindPoseSkinningError(const FSkeletalMeshLOD& LOD, const FSkeleton& Skeleton)
{
    float MaxError = 0.0f;

    for (const FSkeletalVertex& Src : LOD.Vertices)
    {
        FVector SkinnedPos(0.0f, 0.0f, 0.0f);

        for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
        {
            const uint16 BoneIndex = Src.BoneIndices[InfluenceIndex];
            const float  Weight    = Src.BoneWeights[InfluenceIndex];

            if (Weight <= 0.0f)
            {
                continue;
            }

            if (BoneIndex >= Skeleton.Bones.size())
            {
                continue;
            }

            const FMatrix SkinningMatrix = Skeleton.Bones[BoneIndex].InverseBindPose * Skeleton.Bones[BoneIndex].GlobalBindPose;

            SkinnedPos += (Src.Pos * SkinningMatrix) * Weight;
        }

        const float Error = (SkinnedPos - Src.Pos).Length();
        MaxError          = (std::max)(MaxError, Error);
    }

    return MaxError;
}
