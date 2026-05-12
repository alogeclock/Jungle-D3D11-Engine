#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Mesh/SkeletalMeshAsset.h"

struct FFbxImportContext
{
    FSkeletalImportSummary Summary;

    TMap<FbxSurfaceMaterial*, int32> FbxMaterialToIndex;

    // skeletal import summary에 warning 항목을 하나 추가한다.
    void AddWarning(ESkeletalImportWarningType Type, const FString& Message)
    {
        FSkeletalImportWarning Warning;
        Warning.Type    = Type;
        Warning.Message = Message;
        Summary.Warnings.push_back(Warning);
    }
};
