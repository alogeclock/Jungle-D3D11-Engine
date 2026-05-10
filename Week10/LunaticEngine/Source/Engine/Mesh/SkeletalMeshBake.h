#pragma once
#include "SkeletalMeshAsset.h"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"

namespace SkeletalMeshBake
{
    // 현재 엔진이 기대하는 SkeletalMesh Bake 파일 식별자.
    static constexpr uint32 Magic = 0x534B4D31;
    // 현재 엔진이 지원하는 SkeletalMesh Bake 포맷 버전.
    // Version 4: multi UV + animation/morph + import summary + FbxPose/material texture import 대응.
    static constexpr uint32 Version = 4;

    bool Save(const FString& BakePath, FSkeletalMesh& Mesh, TArray<FStaticMaterial>& Materials);
    bool Load(const FString& BakePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
}

// ============================================================================
// FSkeletalMeshBakeHeader
//
// .skm 파일 맨 앞에 저장되는 헤더.
// 이 파일이 정말 SkeletalMesh Bake 파일인지, 현재 코드와 호환되는 버전인지
// 확인하기 위해 사용한다.
// ============================================================================
struct FSkeletalMeshBakeHeader
{
    uint32 Magic   = SkeletalMeshBake::Magic;
    uint32 Version = SkeletalMeshBake::Version;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshBakeHeader& Header)
    {
        Ar << Header.Magic;
        Ar << Header.Version;
        return Ar;
    }
};
