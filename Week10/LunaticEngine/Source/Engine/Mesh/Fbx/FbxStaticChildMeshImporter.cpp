#include "Mesh/Fbx/FbxStaticChildMeshImporter.h"

#include "Engine/Platform/Paths.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxStaticMeshImporter.h"
#include "Mesh/StaticMeshBake.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

namespace
{
    static FString MakeSafeAssetName(FString Name)
    {
        if (Name.empty())
        {
            Name = "Mesh";
        }

        for (char& C : Name)
        {
            const bool bAlphaNumeric = (C >= '0' && C <= '9') || (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z');
            if (!bAlphaNumeric && C != '_' && C != '-')
            {
                C = '_';
            }
        }
        return Name;
    }

    static FString MakeGeneratedStaticMeshPath(const FString& SourceFbxPath, FbxNode* MeshNode, const char* KindSuffix)
    {
        const std::filesystem::path SourcePath(FPaths::ToWide(SourceFbxPath));
        const FString               SourceStem = FPaths::ToUtf8(SourcePath.stem().wstring());
        const FString               NodeName   = MeshNode && MeshNode->GetName() ? FString(MeshNode->GetName()) : FString("Mesh");
        const FString               SafeName   = MakeSafeAssetName(SourceStem + "_" + NodeName + "_" + FString(KindSuffix ? KindSuffix : "Static"));

        std::filesystem::path BakePath = SourcePath.parent_path() / L"Cache" / FPaths::ToWide(SafeName);
        BakePath                       += L".bin";
        return FPaths::ToUtf8(BakePath.lexically_normal().generic_wstring());
    }

    static bool SaveStaticMeshAsset(const FString& AssetPath, FStaticMesh& Mesh, TArray<FStaticMaterial>& Materials)
    {
        const std::filesystem::path DiskPath(FPaths::ToWide(FPaths::ConvertRelativePathToFull(AssetPath)));
        std::filesystem::create_directories(DiskPath.parent_path());

        FWindowsBinWriter Writer(AssetPath);
        if (!Writer.IsValid())
        {
            return false;
        }

        FStaticMeshBakeHeader Header;
        Header.Magic   = StaticMeshBake::Magic;
        Header.Version = StaticMeshBake::Version;

        Writer << Header;
        Mesh.Serialize(Writer);
        Writer << Materials;
        return true;
    }

    static bool ImportSingleNodeStaticMesh(
        FbxNode*           MeshNode,
        const FString&     SourceFbxPath,
        bool               bKeepNodeLocalSpace,
        const char*        KindSuffix,
        FString&           OutStaticMeshAssetPath,
        FFbxImportContext& BuildContext
        )
    {
        if (!MeshNode || !MeshNode->GetMesh())
        {
            return false;
        }

        TArray<FbxNode*> MeshNodes;
        MeshNodes.push_back(MeshNode);

        FStaticMesh StaticMesh;
        TArray<FStaticMaterial> Materials;
        FFbxImportContext LocalContext;
        LocalContext.Summary.SourcePath = SourceFbxPath;
        const bool bImported = bKeepNodeLocalSpace ? FFbxStaticMeshImporter::ImportMeshNodesLocal(MeshNodes, SourceFbxPath, StaticMesh, Materials, LocalContext)
        : FFbxStaticMeshImporter::ImportMeshNodes(MeshNodes, SourceFbxPath, StaticMesh, Materials, LocalContext);

        for (const FSkeletalImportWarning& Warning : LocalContext.Summary.Warnings)
        {
            BuildContext.Summary.Warnings.push_back(Warning);
        }

        if (!bImported)
        {
            return false;
        }

        OutStaticMeshAssetPath  = MakeGeneratedStaticMeshPath(SourceFbxPath, MeshNode, KindSuffix);
        StaticMesh.PathFileName = SourceFbxPath;

        if (!SaveStaticMeshAsset(OutStaticMeshAssetPath, StaticMesh, Materials))
        {
            BuildContext.AddWarning(ESkeletalImportWarningType::StaticChildOfBone, "Failed to save generated static mesh asset: " + OutStaticMeshAssetPath);
            OutStaticMeshAssetPath.clear();
            return false;
        }

        return true;
    }
}

bool FFbxStaticChildMeshImporter::ImportAttachedStaticMesh(
    FbxNode*           MeshNode,
    const FString&     SourceFbxPath,
    FString&           OutStaticMeshAssetPath,
    FFbxImportContext& BuildContext
    )
{
    return ImportSingleNodeStaticMesh(MeshNode, SourceFbxPath, true, "Attached", OutStaticMeshAssetPath, BuildContext);
}

bool FFbxStaticChildMeshImporter::ImportLooseStaticMesh(
    FbxNode*           MeshNode,
    const FString&     SourceFbxPath,
    FString&           OutStaticMeshAssetPath,
    FFbxImportContext& BuildContext
    )
{
    return ImportSingleNodeStaticMesh(MeshNode, SourceFbxPath, false, "Loose", OutStaticMeshAssetPath, BuildContext);
}
