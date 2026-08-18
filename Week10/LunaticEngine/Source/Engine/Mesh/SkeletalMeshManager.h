#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/SkeletalMesh.h"

// ============================================================================
// FSkeletalMeshManager
//
// SkeletalMesh 리소스 로딩 진입점.
// ============================================================================
class FSkeletalMeshManager
{
public:
    // .fbx 경로를 .skm으로 변환
    static FString GetBakedFilePath(const FString& SourcePath);

    // 스켈레탈 메시를 로드하는 외부 노출 함수
    static USkeletalMesh* LoadSkeletalMesh(const FString& PathFileName);

private:
    static bool LoadBakedSkeletalMesh(const FString& BakedPath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
    static bool SaveBakedSkeletalMesh(const FString& BakedPath, FSkeletalMesh& Mesh, TArray<FStaticMaterial>& Materials);

private:
    static TMap<FString, USkeletalMesh*> SkeletalMeshCache;
};
