#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Math/Matrix.h"
#include "Mesh/SkeletalMeshAsset.h"

class FFbxAnimationImporter
{
public:
    // FBX animation stack을 skeleton bone track 기반 animation clip으로 import한다.
    static void ImportAnimations(
        FbxScene*                       Scene,
        const TMap<FbxNode*, int32>&    BoneNodeToIndex,
        const FMatrix&                  ReferenceMeshBindInverse,
        const FSkeleton&                Skeleton,
        TArray<FSkeletalAnimationClip>& OutAnimations
        );
};
