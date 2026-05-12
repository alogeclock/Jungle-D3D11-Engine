#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Math/Matrix.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/StaticMeshAsset.h"
#include "Mesh/Fbx/FbxMorphTargetImporter.h"
#include "Mesh/Fbx/FbxSkeletalMeshClassifier.h"

struct FFbxImportContext;

class FFbxSkeletalMeshLODBuilder
{
public:
    // LOD에 속한 skeletal import mesh node들을 하나의 FSkeletalMeshLOD로 빌드한다.
    static bool Build(
        FbxScene*                                 Scene,
        const FString&                            SourcePath,
        const TArray<FFbxSkeletalImportMeshNode>& MeshNodes,
        const TMap<FbxNode*, int32>&              BoneNodeToIndex,
        const FMatrix&                            ReferenceMeshBindInverse,
        const FSkeleton&                          Skeleton,
        TArray<FStaticMaterial>&                  OutMaterials,
        FSkeletalMeshLOD&                         OutLOD,
        TArray<FFbxImportedMorphSourceVertex>*    OutMorphSources,
        FFbxImportContext&                        BuildContext
        );
};
