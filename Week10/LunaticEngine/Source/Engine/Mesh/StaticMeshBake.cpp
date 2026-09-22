#include "Mesh/StaticMeshBake.h"

#include "Mesh/StaticMesh.h"
#include "Serialization/WindowsArchive.h"

bool StaticMeshBake::Save(const FString& BakePath, UStaticMesh& Mesh)
{
    FWindowsBinWriter Writer(BakePath);
    if (!Writer.IsValid())
    {
        return false;
    }

    FStaticMeshBakeHeader Header;
    Header.Magic   = StaticMeshBake::Magic;
    Header.Version = StaticMeshBake::Version;

    Writer << Header;
    Mesh.Serialize(Writer);

    return true;
}

bool StaticMeshBake::Load(const FString& BakePath, UStaticMesh& OutMesh)
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

    OutMesh.Serialize(Reader);
    return true;
}
