#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/StaticMesh.h"
#include "Serialization/Archive.h"
#include "Serialization/WindowsArchive.h"

namespace StaticMeshBake
{
    // StaticMesh cache 파일 식별자. OBJ/FBX static cache가 서로 충돌하지 않도록 SourceKind도 같이 저장한다.
    // Version 3: StaticMesh multi UV vertex layout 추가.
    static constexpr uint32 Magic   = 0x53544D32; // 'STM2'
    static constexpr uint32 Version = 3;

    enum class ESourceKind : uint32
    {
        Unknown = 0,
        Obj = 1,
        FbxStatic = 2,
    };

    bool Save(const FString& BakePath, UStaticMesh& Mesh, ESourceKind SourceKind);

    // ExpectedSourceKind가 Unknown이면 SourceKind 검사를 생략한다.
    bool Load(const FString& BakePath, UStaticMesh& OutMesh, ESourceKind ExpectedSourceKind = ESourceKind::Unknown);
}

struct FStaticMeshBakeHeader
{
    uint32 Magic      = StaticMeshBake::Magic;
    uint32 Version    = StaticMeshBake::Version;
    uint32 SourceKind = static_cast<uint32>(StaticMeshBake::ESourceKind::Unknown);

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshBakeHeader& Header)
    {
        Ar << Header.Magic;
        Ar << Header.Version;
        Ar << Header.SourceKind;
        return Ar;
    }
};

inline bool StaticMeshBake::Save(const FString& BakePath, UStaticMesh& Mesh, ESourceKind SourceKind)
{
    FWindowsBinWriter Writer(BakePath);
    if (!Writer.IsValid())
    {
        return false;
    }

    FStaticMeshBakeHeader Header;
    Header.Magic      = StaticMeshBake::Magic;
    Header.Version    = StaticMeshBake::Version;
    Header.SourceKind = static_cast<uint32>(SourceKind);

    Writer << Header;
    Mesh.Serialize(Writer);
    return true;
}

inline bool StaticMeshBake::Load(const FString& BakePath, UStaticMesh& OutMesh, ESourceKind ExpectedSourceKind)
{
    FWindowsBinReader Reader(BakePath);
    if (!Reader.IsValid())
    {
        return false;
    }

    FStaticMeshBakeHeader Header;
    Reader << Header;

    if (Header.Magic != StaticMeshBake::Magic)
    {
        return false;
    }

    if (Header.Version != StaticMeshBake::Version)
    {
        return false;
    }

    if (ExpectedSourceKind != ESourceKind::Unknown && Header.SourceKind != static_cast<uint32>(ExpectedSourceKind))
    {
        return false;
    }

    OutMesh.Serialize(Reader);
    return true;
}
