#pragma once

#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"

class UStaticMesh;

namespace StaticMeshBake
{
    // StaticMesh .bin cache 식별자. Legacy raw UStaticMesh::Serialize() cache와 구분한다.
    static constexpr uint32 Magic = 0x53544D31; // 'STM1'
    static constexpr uint32 Version = 1;

    bool Save(const FString& BakePath, UStaticMesh& Mesh);
    bool Load(const FString& BakePath, UStaticMesh& OutMesh);
}

struct FStaticMeshBakeHeader
{
    uint32 Magic   = StaticMeshBake::Magic;
    uint32 Version = StaticMeshBake::Version;

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshBakeHeader& Header)
    {
        Ar << Header.Magic;
        Ar << Header.Version;
        return Ar;
    }
};
