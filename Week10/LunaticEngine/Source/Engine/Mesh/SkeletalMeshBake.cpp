#include "SkeletalMeshBake.h"

#include "Serialization/WindowsArchive.h"

bool SkeletalMeshBake::Save(const FString& BakePath, FSkeletalMesh& Mesh, TArray<FStaticMaterial>& Materials)
{
    FWindowsBinWriter Writer(BakePath);

    if (!Writer.IsValid())
    {
        return false;
    }

    FSkeletalMeshBakeHeader Header;
    Header.Magic   = SkeletalMeshBake::Magic;
    Header.Version = SkeletalMeshBake::Version;

    Writer << Header;
    Mesh.Serialize(Writer);
    Writer << Materials;

    return true;
}

bool SkeletalMeshBake::Load(const FString& BakePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    FWindowsBinReader Reader(BakePath);

    if (!Reader.IsValid())
    {
        return false;
    }

    FSkeletalMeshBakeHeader Header;
    Reader << Header;

    if (Header.Magic != SkeletalMeshBake::Magic)
    {
        return false;
    }

    if (Header.Version != SkeletalMeshBake::Version)
    {
        return false;
    }

    OutMesh.Serialize(Reader);
    Reader << OutMaterials;

    return true;
}
