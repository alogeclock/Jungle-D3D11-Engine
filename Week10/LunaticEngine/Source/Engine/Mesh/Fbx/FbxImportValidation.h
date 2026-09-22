#pragma once

#include "Mesh/SkeletalMeshAsset.h"

class FFbxImportValidation
{
public:
    // bind pose skinning 결과가 원본 vertex와 얼마나 다른지 최대 오차를 계산한다.
    static float ValidateBindPoseSkinningError(const FSkeletalMeshLOD& LOD, const FSkeleton& Skeleton);
};
