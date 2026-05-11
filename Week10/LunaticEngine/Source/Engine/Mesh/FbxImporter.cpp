#include "Mesh/FbxImporter.h"

#include "Mesh/StaticMeshAsset.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Materials/MaterialManager.h"
#include "Engine/Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <fbxsdk.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{
    // OutMessage 포인터가 유효할 때 FString 메시지를 기록한다.
    void SetMessage(FString* OutMessage, const FString& Message)
    {
        if (OutMessage)
        {
            *OutMessage = Message;
        }
    }

    // OutMessage 포인터가 유효할 때 C 문자열 메시지를 FString으로 기록한다.
    void SetMessage(FString* OutMessage, const char* Message)
    {
        if (OutMessage)
        {
            *OutMessage = Message ? Message : "";
        }
    }

    struct FFbxSceneHandle
    {
        FbxManager* Manager = nullptr;
        FbxScene*   Scene   = nullptr;

        // FBX SDK Manager를 파괴해 Scene까지 함께 정리한다.
        ~FFbxSceneHandle()
        {
            if (Manager)
            {
                Manager->Destroy();
                Manager = nullptr;
                Scene   = nullptr;
            }
        }
    };

    struct FImportedBoneWeight
    {
        int32 BoneIndex = 0;
        float Weight    = 0.0f;
    };

    struct FImportBuildContext
    {
        FSkeletalImportSummary Summary;

        // Skeletal import 중 발생한 경고를 ImportSummary에 누적한다.
        void AddWarning(ESkeletalImportWarningType Type, const FString& Message)
        {
            FSkeletalImportWarning Warning;
            Warning.Type    = Type;
            Warning.Message = Message;
            Summary.Warnings.push_back(Warning);
        }
    };

    struct FFbxImportedMaterialInfo
    {
        FString SlotName;

        FString DiffuseTexture;
        FString NormalTexture;
        FString RoughnessTexture;
        FString MetallicTexture;
        FString EmissiveTexture;
        FString OpacityTexture;

        FVector4 BaseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    };

    // 파일명으로 쓰기 어려운 문자를 안전한 '_' 문자로 바꾼다.
    static FString MakeSafeAssetName(const FString& Name)
    {
        FString Result = Name.empty() ? FString("None") : Name;

        for (char& C : Result)
        {
            const bool bAlphaNumeric = (C >= '0' && C <= '9') || (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z');

            if (!bAlphaNumeric && C != '_' && C != '-')
            {
                C = '_';
            }
        }

        return Result;
    }

    static uint32 FloatToStableBits(float Value)
    {
        if (Value == 0.0f)
        {
            Value = 0.0f;
        }

        uint32 Bits = 0;
        static_assert(sizeof(Bits) == sizeof(Value), "float and uint32 size mismatch");
        std::memcpy(&Bits, &Value, sizeof(float));
        return Bits;
    }

    static void HashCombineSizeT(std::size_t& Seed, std::size_t Value)
    {
        Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
    }

    static void HashCombineUInt32(std::size_t& Seed, uint32 Value)
    {
        HashCombineSizeT(Seed, static_cast<std::size_t>(Value));
    }

    static void HashCombineInt32(std::size_t& Seed, int32 Value)
    {
        HashCombineSizeT(Seed, static_cast<std::size_t>(static_cast<uint32>(Value)));
    }

    static void HashCombineUInt16(std::size_t& Seed, uint16 Value)
    {
        HashCombineSizeT(Seed, static_cast<std::size_t>(Value));
    }

    static void HashCombinePointer(std::size_t& Seed, const void* Pointer)
    {
        HashCombineSizeT(Seed, std::hash<const void*> {}(Pointer));
    }

    static std::array<uint32, 2> MakeVector2Bits(const FVector2& V)
    {
        return { FloatToStableBits(V.X), FloatToStableBits(V.Y) };
    }

    static std::array<uint32, 3> MakeVector3Bits(const FVector& V)
    {
        return { FloatToStableBits(V.X), FloatToStableBits(V.Y), FloatToStableBits(V.Z) };
    }

    static std::array<uint32, 4> MakeVector4Bits(const FVector4& V)
    {
        return { FloatToStableBits(V.X), FloatToStableBits(V.Y), FloatToStableBits(V.Z), FloatToStableBits(V.W) };
    }

    struct FStaticVertexDedupKey
    {
        const FbxMesh* Mesh              = nullptr;
        int32          ControlPointIndex = -1;

        std::array<uint32, 3> Position = {};
        std::array<uint32, 3> Normal   = {};
        std::array<uint32, 2> UV       = {};
        std::array<uint32, 4> Color    = {};
        std::array<uint32, 4> Tangent  = {};

        bool operator==(const FStaticVertexDedupKey& Other) const
        {
            return Mesh == Other.Mesh && ControlPointIndex == Other.ControlPointIndex && Position == Other.Position && Normal == Other.Normal && UV == Other.UV
            && Color == Other.Color && Tangent == Other.Tangent;
        }
    };

    struct FStaticVertexDedupKeyHasher
    {
        std::size_t operator()(const FStaticVertexDedupKey& Key) const
        {
            std::size_t Seed = 0;

            HashCombinePointer(Seed, Key.Mesh);
            HashCombineInt32(Seed, Key.ControlPointIndex);

            for (uint32 Value : Key.Position) HashCombineUInt32(Seed, Value);
            for (uint32 Value : Key.Normal) HashCombineUInt32(Seed, Value);
            for (uint32 Value : Key.UV) HashCombineUInt32(Seed, Value);
            for (uint32 Value : Key.Color) HashCombineUInt32(Seed, Value);
            for (uint32 Value : Key.Tangent) HashCombineUInt32(Seed, Value);

            return Seed;
        }
    };

    static FStaticVertexDedupKey MakeStaticVertexDedupKey(const FNormalVertex& Vertex, const FbxMesh* Mesh, int32 ControlPointIndex)
    {
        FStaticVertexDedupKey Key;
        Key.Mesh              = Mesh;
        Key.ControlPointIndex = ControlPointIndex;
        Key.Position          = MakeVector3Bits(Vertex.pos);
        Key.Normal            = MakeVector3Bits(Vertex.normal);
        Key.UV                = MakeVector2Bits(Vertex.tex);
        Key.Color             = MakeVector4Bits(Vertex.color);
        Key.Tangent           = MakeVector4Bits(Vertex.tangent);
        return Key;
    }

    static uint32 FindOrAddStaticVertex(
        const FNormalVertex&                                                            Vertex,
        const FbxMesh*                                                                  Mesh,
        int32                                                                           ControlPointIndex,
        std::unordered_map<FStaticVertexDedupKey, uint32, FStaticVertexDedupKeyHasher>& VertexToIndex,
        TArray<FNormalVertex>&                                                          OutVertices,
        bool&                                                                           bOutAddedNewVertex
        )
    {
        const FStaticVertexDedupKey Key = MakeStaticVertexDedupKey(Vertex, Mesh, ControlPointIndex);

        auto It = VertexToIndex.find(Key);
        if (It != VertexToIndex.end())
        {
            bOutAddedNewVertex = false;
            return It->second;
        }

        const uint32 NewIndex = static_cast<uint32>(OutVertices.size());
        OutVertices.push_back(Vertex);
        VertexToIndex.emplace(Key, NewIndex);
        bOutAddedNewVertex = true;
        return NewIndex;
    }

    struct FSkeletalVertexDedupKey
    {
        const FbxMesh* Mesh              = nullptr;
        int32          ControlPointIndex = -1;
        int32          MaterialIndex     = 0;

        std::array<uint32, 3>                                 Position    = {};
        std::array<uint32, 3>                                 Normal      = {};
        std::array<std::array<uint32, 2>, 4>                  UV          = {};
        std::array<uint32, 4>                                 Color       = {};
        std::array<uint32, 4>                                 Tangent     = {};
        uint8                                                 NumUV       = 0;
        std::array<uint16, MAX_SKELETAL_MESH_BONE_INFLUENCES> BoneIndices = {};
        std::array<uint32, MAX_SKELETAL_MESH_BONE_INFLUENCES> BoneWeights = {};

        bool operator==(const FSkeletalVertexDedupKey& Other) const
        {
            return Mesh == Other.Mesh && ControlPointIndex == Other.ControlPointIndex && MaterialIndex == Other.MaterialIndex && Position == Other.Position &&
            Normal == Other.Normal && UV == Other.UV && Color == Other.Color && Tangent == Other.Tangent && NumUV == Other.NumUV && BoneIndices == Other.
            BoneIndices && BoneWeights == Other.BoneWeights;
        }
    };

    struct FSkeletalVertexDedupKeyHasher
    {
        std::size_t operator()(const FSkeletalVertexDedupKey& Key) const
        {
            std::size_t Seed = 0;

            HashCombinePointer(Seed, Key.Mesh);
            HashCombineInt32(Seed, Key.ControlPointIndex);
            HashCombineInt32(Seed, Key.MaterialIndex);

            for (uint32 Value : Key.Position) HashCombineUInt32(Seed, Value);
            for (uint32 Value : Key.Normal) HashCombineUInt32(Seed, Value);
            for (uint32 Value : Key.Color) HashCombineUInt32(Seed, Value);

            for (const std::array<uint32, 2>& UV : Key.UV)
            {
                HashCombineUInt32(Seed, UV[0]);
                HashCombineUInt32(Seed, UV[1]);
            }

            HashCombineInt32(Seed, static_cast<int32>(Key.NumUV));

            for (uint32 Value : Key.Tangent) HashCombineUInt32(Seed, Value);

            for (uint16 Value : Key.BoneIndices) HashCombineUInt16(Seed, Value);
            for (uint32 Value : Key.BoneWeights) HashCombineUInt32(Seed, Value);

            return Seed;
        }
    };

    static FSkeletalVertexDedupKey MakeSkeletalVertexDedupKey(const FSkeletalVertex& Vertex, const FbxMesh* Mesh, int32 ControlPointIndex, int32 MaterialIndex)
    {
        FSkeletalVertexDedupKey Key;
        Key.Mesh              = Mesh;
        Key.ControlPointIndex = ControlPointIndex;
        Key.MaterialIndex     = MaterialIndex;
        Key.Position          = MakeVector3Bits(Vertex.Pos);
        Key.Normal            = MakeVector3Bits(Vertex.Normal);
        Key.Color             = MakeVector4Bits(Vertex.Color);
        Key.Tangent           = MakeVector4Bits(Vertex.Tangent);
        Key.NumUV             = Vertex.NumUVs;

        for (int32 UVIndex = 0; UVIndex < Vertex.NumUVs; ++UVIndex)
        {
            Key.UV[UVIndex] = MakeVector2Bits(Vertex.UV[UVIndex]);
        }

        for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
        {
            Key.BoneIndices[InfluenceIndex] = Vertex.BoneIndices[InfluenceIndex];
            Key.BoneWeights[InfluenceIndex] = FloatToStableBits(Vertex.BoneWeights[InfluenceIndex]);
        }

        return Key;
    }

    static uint32 FindOrAddSkeletalVertex(
        const FSkeletalVertex&                                                              Vertex,
        const FbxMesh*                                                                      Mesh,
        int32                                                                               ControlPointIndex,
        int32                                                                               MaterialIndex,
        std::unordered_map<FSkeletalVertexDedupKey, uint32, FSkeletalVertexDedupKeyHasher>& VertexToIndex,
        TArray<FSkeletalVertex>&                                                            OutVertices,
        bool&                                                                               bOutAddedNewVertex
        )
    {
        const FSkeletalVertexDedupKey Key = MakeSkeletalVertexDedupKey(Vertex, Mesh, ControlPointIndex, MaterialIndex);

        auto It = VertexToIndex.find(Key);
        if (It != VertexToIndex.end())
        {
            bOutAddedNewVertex = false;
            return It->second;
        }

        const uint32 NewIndex = static_cast<uint32>(OutVertices.size());
        OutVertices.push_back(Vertex);
        VertexToIndex.emplace(Key, NewIndex);
        bOutAddedNewVertex = true;
        return NewIndex;
    }

    // FBX texture property에서 외부 texture 경로를 추출해 엔진 asset 상대경로로 변환한다.
    static bool TryGetFbxTexturePathFromProperty(const FbxProperty& Property, const FString& SourceFbxPath, FString& OutTexturePath)
    {
        if (!Property.IsValid())
        {
            return false;
        }

        const int32 TextureCount = Property.GetSrcObjectCount<FbxFileTexture>();

        for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
        {
            FbxFileTexture* FileTexture = Property.GetSrcObject<FbxFileTexture>(TextureIndex);
            if (!FileTexture)
            {
                continue;
            }

            const char* RelativeName = FileTexture->GetRelativeFileName();
            const char* FileName     = FileTexture->GetFileName();

            FString TexturePath;

            if (RelativeName && RelativeName[0] != '\0')
            {
                TexturePath = RelativeName;
            }
            else if (FileName && FileName[0] != '\0')
            {
                TexturePath = FileName;
            }

            if (TexturePath.empty())
            {
                continue;
            }

            if (TexturePath.rfind("Asset/", 0) == 0)
            {
                OutTexturePath = TexturePath;
            }
            else
            {
                OutTexturePath = FPaths::ResolveAssetPath(SourceFbxPath, TexturePath);
            }

            return true;
        }

        return false;
    }

    // FBX material의 color property를 FVector4로 읽는다.
    static bool TryGetFbxColorProperty(FbxSurfaceMaterial* Material, const char* PropertyName, FVector4& OutColor)
    {
        if (!Material || !PropertyName)
        {
            return false;
        }

        FbxProperty Property = Material->FindProperty(PropertyName);
        if (!Property.IsValid())
        {
            return false;
        }

        const FbxDouble3 Value = Property.Get<FbxDouble3>();
        OutColor               = FVector4(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2]), 1.0f);
        return true;
    }

    // FBX material에서 엔진 material 생성에 필요한 texture path와 base color를 추출한다.
    static FFbxImportedMaterialInfo ExtractFbxMaterialInfo(FbxSurfaceMaterial* FbxMaterial, const FString& SourceFbxPath)
    {
        FFbxImportedMaterialInfo Info;
        Info.SlotName = FbxMaterial ? FbxMaterial->GetName() : "None";

        if (!FbxMaterial)
        {
            return Info;
        }

        TryGetFbxColorProperty(FbxMaterial, FbxSurfaceMaterial::sDiffuse, Info.BaseColor);

        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse), SourceFbxPath, Info.DiffuseTexture);
        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap), SourceFbxPath, Info.NormalTexture);

        if (Info.NormalTexture.empty())
        {
            TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sBump), SourceFbxPath, Info.NormalTexture);
        }

        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sEmissive), SourceFbxPath, Info.EmissiveTexture);
        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty("Roughness"), SourceFbxPath, Info.RoughnessTexture);

        if (!TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty("Metalness"), SourceFbxPath, Info.MetallicTexture))
        {
            TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty("Metallic"), SourceFbxPath, Info.MetallicTexture);
        }

        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor), SourceFbxPath, Info.OpacityTexture);

        return Info;
    }

    // FBX source와 slot 이름을 이용해 충돌 가능성이 낮은 자동 material asset path를 만든다.
    static FString MakeAutoFbxMaterialPath(const FString& SourceFbxPath, const FString& SlotName)
    {
        const std::filesystem::path SourcePath(FPaths::ToWide(SourceFbxPath));

        FString SourceAssetName = FPaths::ToUtf8(SourcePath.stem().wstring());
        if (SourceAssetName.empty())
        {
            SourceAssetName = "UnknownFbx";
        }

        return "Asset/Materials/Auto/Fbx/" + MakeSafeAssetName(SourceAssetName) + "/" + MakeSafeAssetName(SlotName) + ".mat";
    }

    // 추출한 FBX material 정보를 엔진 .mat JSON 파일로 저장하고 material asset path를 반환한다.
    static FString ConvertFbxMaterialInfoToMat(const FFbxImportedMaterialInfo& Info, const FString& SourceFbxPath, FImportBuildContext& BuildContext)
    {
        const FString MatPath = MakeAutoFbxMaterialPath(SourceFbxPath, Info.SlotName);
        const FString FullDiskPath = FPaths::ConvertRelativePathToFull(MatPath);
        const std::filesystem::path DiskPath(FPaths::ToWide(FullDiskPath));

        if (std::filesystem::exists(DiskPath))
        {
            return MatPath;
        }

        std::filesystem::create_directories(DiskPath.parent_path());

        json::JSON JsonData;
        JsonData["PathFileName"] = MatPath;
        JsonData["Origin"]       = "FbxImport";
        JsonData["ShaderPath"]   = "Shaders/Geometry/UberLit.hlsl";
        JsonData["RenderPass"]   = "Opaque";

        if (!Info.DiffuseTexture.empty())
        {
            JsonData["Textures"]["DiffuseTexture"] = Info.DiffuseTexture;
        }

        if (!Info.NormalTexture.empty())
        {
            JsonData["Textures"]["NormalTexture"] = Info.NormalTexture;
        }

        if (!Info.RoughnessTexture.empty())
        {
            JsonData["Textures"]["RoughnessTexture"] = Info.RoughnessTexture;
        }

        if (!Info.MetallicTexture.empty())
        {
            JsonData["Textures"]["MetallicTexture"] = Info.MetallicTexture;
        }

        if (!Info.EmissiveTexture.empty())
        {
            JsonData["Textures"]["EmissiveTexture"] = Info.EmissiveTexture;
        }

        if (!Info.OpacityTexture.empty())
        {
            JsonData["Textures"]["Custom0Texture"] = Info.OpacityTexture;
        }

        JsonData["Parameters"]["SectionColor"][0] = Info.BaseColor.X;
        JsonData["Parameters"]["SectionColor"][1] = Info.BaseColor.Y;
        JsonData["Parameters"]["SectionColor"][2] = Info.BaseColor.Z;
        JsonData["Parameters"]["SectionColor"][3] = Info.BaseColor.W;

        std::ofstream File(DiskPath, std::ios::binary);
        if (!File.is_open())
        {
            BuildContext.AddWarning(ESkeletalImportWarningType::UnsupportedMaterialProperty, "Failed to create auto material file: " + MatPath);
            return "None";
        }

        File << JsonData.dump();
        return MatPath;
    }

    // FBX 파일을 SDK Scene으로 로드하고 실패 시 오류 메시지를 채운다.
    static bool LoadFbxScene(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage = nullptr)
    {
        const FString FullPath = FPaths::ConvertRelativePathToFull(SourcePath);

        FbxManager* Manager = FbxManager::Create();
        if (!Manager)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK manager");
            return false;
        }

        FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
        if (!IOSettings)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK IO settings");
            Manager->Destroy();
            return false;
        }

        Manager->SetIOSettings(IOSettings);

        FbxImporter* Importer = FbxImporter::Create(Manager, "");
        if (!Importer)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK importer");
            Manager->Destroy();
            return false;
        }

        if (!Importer->Initialize(FullPath.c_str(), -1, Manager->GetIOSettings()))
        {
            FString Error = "FBX initialize failed: ";
            Error         += Importer->GetStatus().GetErrorString();

            SetMessage(OutMessage, Error);

            Importer->Destroy();
            Manager->Destroy();
            return false;
        }

        FbxScene* Scene = FbxScene::Create(Manager, "ImportScene");
        if (!Scene)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK scene");

            Importer->Destroy();
            Manager->Destroy();
            return false;
        }

        if (!Importer->Import(Scene))
        {
            FString Error = "FBX import failed: ";
            Error         += Importer->GetStatus().GetErrorString();

            SetMessage(OutMessage, Error);

            Importer->Destroy();
            Manager->Destroy();
            return false;
        }

        Importer->Destroy();

        OutScene.Manager = Manager;
        OutScene.Scene   = Scene;

        return true;
    }

    // FBX Scene의 모든 polygon mesh를 삼각형 mesh로 변환한다.
    static void TriangulateScene(FbxManager* Manager, FbxScene* Scene)
    {
        if (!Manager || !Scene)
        {
            return;
        }

        FbxGeometryConverter Converter(Manager);
        Converter.Triangulate(Scene, true);
    }

    // FBX 노드 트리를 재귀 순회하며 mesh node만 수집한다.
    static void CollectMeshNodes(FbxNode* Node, TArray<FbxNode*>& OutMeshNodes)
    {
        if (!Node)
        {
            return;
        }

        FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
        if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            OutMeshNodes.push_back(Node);
        }

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            CollectMeshNodes(Node->GetChild(ChildIndex), OutMeshNodes);
        }
    }

    // Mesh에 skin deformer가 하나 이상 있는지 확인한다.
    static bool MeshHasSkin(FbxMesh* Mesh)
    {
        return Mesh && Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
    }

    // FBX node가 skeleton bone node인지 확인한다.
    static bool IsFbxSkeletonNode(FbxNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
        return Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton;
    }

    // FBX 행렬을 엔진 FMatrix 형식으로 복사 변환한다.
    static FMatrix ToEngineMatrix(const FbxAMatrix& Source)
    {
        FMatrix Result = FMatrix::Identity;

        for (int32 Row = 0; Row < 4; ++Row)
        {
            for (int32 Col = 0; Col < 4; ++Col)
            {
                Result.M[Row][Col] = static_cast<float>(Source.Get(Row, Col));
            }
        }

        return Result;
    }

    // FBX pose matrix를 엔진 FMatrix 형식으로 복사 변환한다.
    static FMatrix ToEngineMatrix(const FbxMatrix& Source)
    {
        FMatrix Result = FMatrix::Identity;

        for (int32 Row = 0; Row < 4; ++Row)
        {
            for (int32 Col = 0; Col < 4; ++Col)
            {
                Result.M[Row][Col] = static_cast<float>(Source.Get(Row, Col));
            }
        }

        return Result;
    }

    // FBX node의 geometric translation/rotation/scale transform을 가져온다.
    static FbxAMatrix GetNodeGeometryTransform(FbxNode* Node)
    {
        FbxAMatrix GeometryTransform;
        GeometryTransform.SetIdentity();

        if (!Node)
        {
            return GeometryTransform;
        }

        GeometryTransform.SetT(Node->GetGeometricTranslation(FbxNode::eSourcePivot));
        GeometryTransform.SetR(Node->GetGeometricRotation(FbxNode::eSourcePivot));
        GeometryTransform.SetS(Node->GetGeometricScaling(FbxNode::eSourcePivot));

        return GeometryTransform;
    }

    // FVector의 각 성분이 지정 허용오차 이하인지 검사한다.
    static bool IsNearlyZeroVector(const FVector& V, float Tolerance = 1.0e-6f)
    {
        return std::fabs(V.X) <= Tolerance && std::fabs(V.Y) <= Tolerance && std::fabs(V.Z) <= Tolerance;
    }

    // FVector4의 각 성분이 지정 허용오차 이하인지 검사한다.
    static bool IsNearlyZeroVector4(const FVector4& V, float Tolerance = 1.0e-6f)
    {
        return std::fabs(V.X) <= Tolerance && std::fabs(V.Y) <= Tolerance && std::fabs(V.Z) <= Tolerance && std::fabs(V.W) <= Tolerance;
    }

    // 배열 안에 특정 FBX node 포인터가 이미 있는지 확인한다.
    static bool ContainsFbxNode(const TArray<FbxNode*>& Nodes, const FbxNode* Node)
    {
        return std::find(Nodes.begin(), Nodes.end(), Node) != Nodes.end();
    }

    // 중복 없이 FBX node 포인터를 배열에 추가한다.
    static void AddUniqueFbxNode(TArray<FbxNode*>& Nodes, FbxNode* Node)
    {
        if (Node && !ContainsFbxNode(Nodes, Node))
        {
            Nodes.push_back(Node);
        }
    }

    // Mesh의 skin cluster가 참조하는 bone link node들을 수집한다.
    static void CollectSkinClusterLinksFromMesh(FbxMesh* Mesh, TArray<FbxNode*>& OutLinkNodes)
    {
        if (!Mesh)
        {
            return;
        }

        const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

        for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
            if (!Skin)
            {
                continue;
            }

            const int32 ClusterCount = Skin->GetClusterCount();

            for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
            {
                FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
                if (!Cluster || !Cluster->GetLink())
                {
                    continue;
                }

                AddUniqueFbxNode(OutLinkNodes, Cluster->GetLink());
            }
        }
    }

    // FBX node가 scene root인지 확인한다.
    static bool IsSceneRootNode(FbxNode* Node)
    {
        return Node && Node->GetParent() == nullptr;
    }

    // Mesh node의 parent chain에서 가장 가까운 skeleton bone node를 찾는다.
    static FbxNode* FindNearestParentSkeletonNode(FbxNode* MeshNode)
    {
        FbxNode* Current = MeshNode ? MeshNode->GetParent() : nullptr;

        while (Current && !IsSceneRootNode(Current))
        {
            if (IsFbxSkeletonNode(Current))
            {
                return Current;
            }

            Current = Current->GetParent();
        }

        return nullptr;
    }

    // 특정 node부터 scene root 직전까지 parent chain을 중복 없이 추가한다.
    static void AddNodeAndParentsUntilSceneRoot(FbxNode* Node, TArray<FbxNode*>& OutNodes)
    {
        FbxNode* Current = Node;

        while (Current && !IsSceneRootNode(Current))
        {
            AddUniqueFbxNode(OutNodes, Current);
            Current = Current->GetParent();
        }
    }

    // import 대상 bone 집합에서 parent가 집합 밖인 root bone들을 찾는다.
    static void FindImportedBoneRoot(const TArray<FbxNode*>& Nodes, TArray<FbxNode*>& OutRoots)
    {
        for (FbxNode* Node : Nodes)
        {
            if (!Node)
            {
                continue;
            }

            FbxNode* Parent = Node->GetParent();

            if (!Parent || !ContainsFbxNode(Nodes, Parent))
            {
                AddUniqueFbxNode(OutRoots, Node);
            }
        }
    }

    // FBX bone node를 Skeleton에 추가하고 import 대상 child bone을 재귀 등록한다.
    static int32 AddImportedBoneRecursive(
        FbxNode*                BoneNode,
        int32                   ParentIndex,
        const TArray<FbxNode*>& ImportedBoneNodes,
        FSkeleton&              OutSkeleton,
        TMap<FbxNode*, int32>&  OutBoneNodeToIndex
        )
    {
        if (!BoneNode || !ContainsFbxNode(ImportedBoneNodes, BoneNode))
        {
            return -1;
        }

        auto Existing = OutBoneNodeToIndex.find(BoneNode);
        if (Existing != OutBoneNodeToIndex.end()) return Existing->second;

        const int32 BoneIndex = static_cast<int32>(OutSkeleton.Bones.size());

        FBoneInfo Bone;
        Bone.Name        = BoneNode->GetName();
        Bone.ParentIndex = ParentIndex;

        const FbxAMatrix LocalBind  = BoneNode->EvaluateLocalTransform();
        const FbxAMatrix GlobalBind = BoneNode->EvaluateGlobalTransform();

        Bone.LocalBindPose   = ToEngineMatrix(LocalBind);
        Bone.GlobalBindPose  = ToEngineMatrix(GlobalBind);
        Bone.InverseBindPose = Bone.GlobalBindPose.GetInverse();

        OutSkeleton.Bones.push_back(Bone);
        OutBoneNodeToIndex[BoneNode] = BoneIndex;

        for (int32 ChildIndex = 0; ChildIndex < BoneNode->GetChildCount(); ++ChildIndex)
        {
            FbxNode* Child = BoneNode->GetChild(ChildIndex);

            if (ContainsFbxNode(ImportedBoneNodes, Child))
            {
                AddImportedBoneRecursive(Child, BoneIndex, ImportedBoneNodes, OutSkeleton, OutBoneNodeToIndex);
            }
        }
        return BoneIndex;
    }

    // skinned mesh의 skin cluster link와 bone parent 아래 rigid mesh를 기준으로 skeleton hierarchy를 구성한다.
    static bool BuildSkeletonFromSkinClusters(
        const TArray<FbxNode*>& SkinnedMeshNodes,
        const TArray<FbxNode*>& AllMeshNodes,
        FSkeleton&              OutSkeleton,
        TMap<FbxNode*, int32>&  OutBoneNodeToIndex
        )
    {
        OutSkeleton.Bones.clear();
        OutBoneNodeToIndex.clear();

        TArray<FbxNode*> LinkNodes;

        for (FbxNode* MeshNode : SkinnedMeshNodes)
        {
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

            if (Mesh && MeshHasSkin(Mesh))
            {
                CollectSkinClusterLinksFromMesh(Mesh, LinkNodes);
            }
        }

        if (LinkNodes.empty())
        {
            return false;
        }

        TArray<FbxNode*> ImportedBoneNodes;

        for (FbxNode* LinkNode : LinkNodes)
        {
            AddNodeAndParentsUntilSceneRoot(LinkNode, ImportedBoneNodes);
        }

        // skin은 없지만 skeleton bone parent 아래에 붙은 mesh를 rigid attachment로 처리할 수 있도록,
        // 해당 parent skeleton bone도 skeleton 후보에 포함한다.
        for (FbxNode* MeshNode : AllMeshNodes)
        {
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

            if (!Mesh || MeshHasSkin(Mesh))
            {
                continue;
            }

            FbxNode* RigidParentBone = FindNearestParentSkeletonNode(MeshNode);
            if (RigidParentBone)
            {
                AddNodeAndParentsUntilSceneRoot(RigidParentBone, ImportedBoneNodes);
            }
        }

        TArray<FbxNode*> RootBones;

        FindImportedBoneRoot(ImportedBoneNodes, RootBones);

        for (FbxNode* RootBone : RootBones)
        {
            AddImportedBoneRecursive(RootBone, -1, ImportedBoneNodes, OutSkeleton, OutBoneNodeToIndex);
        }

        OutSkeleton.RebuildChildren();

        return !OutSkeleton.Bones.empty();
    }

    // node 이름의 LOD 패턴에서 LOD index를 파싱한다.
    static int32 ParseLODIndexFromName(const FString& Name)
    {
        const char* Patterns[] = { "_LOD", "_lod", "LOD", "lod" };

        for (const char* Pattern : Patterns)
        {
            const size_t Pos = Name.rfind(Pattern);
            if (Pos == FString::npos)
            {
                continue;
            }

            size_t DigitPos = Pos + std::strlen(Pattern);

            if (DigitPos < Name.size() && Name[DigitPos] == '_')
            {
                ++DigitPos;
            }

            if (DigitPos >= Name.size() || !std::isdigit(static_cast<unsigned char>(Name[DigitPos])))
            {
                continue;
            }

            int32 LODIndex = 0;

            while (DigitPos < Name.size() && std::isdigit(static_cast<unsigned char>(Name[DigitPos])))
            {
                LODIndex = LODIndex * 10 + (Name[DigitPos] - '0');
                ++DigitPos;
            }

            return LODIndex;
        }

        return 0;
    }

    // LODGroup 또는 node 이름을 기준으로 skeletal mesh LOD index를 결정한다.
    static int32 GetSkeletalMeshLODIndex(FbxNode* MeshNode)
    {
        if (!MeshNode)
        {
            return 0;
        }

        FbxNode* Parent = MeshNode->GetParent();
        if (Parent && Parent->GetNodeAttribute() && Parent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
        {
            for (int32 LODIndex = 0; LODIndex < Parent->GetChildCount(); ++LODIndex)
            {
                if (Parent->GetChild(LODIndex) == MeshNode)
                {
                    return LODIndex;
                }
            }
        }

        return ParseLODIndexFromName(MeshNode->GetName());
    }

    // material 배열이 비어 있으면 기본 None material slot을 추가한다.
    static void GetOrCreateDefaultMaterial(TArray<FStaticMaterial>& OutMaterials)
    {
        if (!OutMaterials.empty())
        {
            return;
        }

        FStaticMaterial DefaultMaterial;
        DefaultMaterial.MaterialSlotName  = "None";
        DefaultMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");

        OutMaterials.push_back(DefaultMaterial);
    }

    // Mesh의 첫 번째 UV set 이름을 반환한다.
    static const char* GetPrimaryUVSetName(FbxMesh* Mesh, FbxStringList& OutUVSetNames)
    {
        if (!Mesh)
        {
            return nullptr;
        }

        Mesh->GetUVSetNames(OutUVSetNames);

        if (OutUVSetNames.GetCount() <= 0)
        {
            return nullptr;
        }

        return OutUVSetNames[0];
    }

    // FBX control point index에서 vertex position을 읽는다.
    static FVector ReadPosition(FbxMesh* Mesh, int32 ControlPointIndex)
    {
        if (!Mesh) return FVector(0.0f, 0.0f, 0.0f);

        const int32 ControlPointCount = Mesh->GetControlPointsCount();

        if (ControlPointIndex < 0 || ControlPointIndex >= ControlPointCount) return FVector(0.0f, 0.0f, 0.0f);

        FbxVector4*      ControlPoint = Mesh->GetControlPoints();
        const FbxVector4 P            = ControlPoint[ControlPointIndex];

        return FVector(static_cast<float>(P[0]), static_cast<float>(P[1]), static_cast<float>(P[2]));
    }

    // polygon corner의 normal을 읽고 성공 여부를 반환한다.
    static bool TryReadNormal(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex, FVector& OutNormal)
    {
        FbxVector4 Normal;

        if (Mesh && Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, Normal))
        {
            OutNormal = FVector(static_cast<float>(Normal[0]), static_cast<float>(Normal[1]), static_cast<float>(Normal[2])).Normalized();
            return true;
        }

        OutNormal = FVector(0.0f, 0.0f, 1.0f);
        return false;
    }

    // 삼각형 세 점으로 face normal fallback을 계산한다.
    static FVector ComputeTriangleNormal(const FVector& P0, const FVector& P1, const FVector& P2)
    {
        const FVector E0 = P1 - P0;
        const FVector E1 = P2 - P0;
        const FVector N  = E0.Cross(E1);
        if (N.IsNearlyZero(1.0e-6f))
        {
            return FVector(0.0f, 0.0f, 1.0f);
        }
        return N.Normalized();
    }

    // Mesh의 첫 번째 UV set에서 polygon corner UV를 읽는다.
    static FVector2 ReadUV(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex)
    {
        if (!Mesh)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxStringList UVSetNames;
        const char*   UVSetName = GetPrimaryUVSetName(Mesh, UVSetNames);

        if (!UVSetName)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxVector2 UV;
        bool       bUnmapped = false;

        if (Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, UV, bUnmapped))
        {
            return FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
        }

        return FVector2(0.0f, 0.0f);
    }

    // Mesh가 가진 UV set 개수를 반환한다.
    static int32 GetUVSetCount(FbxMesh* Mesh)
    {
        if (!Mesh)
        {
            return 0;
        }

        FbxStringList UVSetNames;
        Mesh->GetUVSetNames(UVSetNames);
        return static_cast<int32>(UVSetNames.GetCount());
    }

    // 지정 UV channel에서 polygon corner UV를 읽는다.
    static FVector2 ReadUVByChannel(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex, int32 ChannelIndex)
    {
        if (!Mesh)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxStringList UVSetNames;
        Mesh->GetUVSetNames(UVSetNames);

        if (ChannelIndex < 0 || ChannelIndex >= UVSetNames.GetCount())
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxVector2 UV;
        bool       bUnmapped = false;

        if (Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetNames[ChannelIndex], UV, bUnmapped) && !bUnmapped)
        {
            return FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
        }

        return FVector2(0.0f, 0.0f);
    }

    // FBX vertex color layer에서 control point 또는 polygon vertex 색상을 읽는다.
    static FVector4 ReadVertexColor(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex)
    {
        if (!Mesh || !Mesh->GetLayer(0))
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        FbxLayerElementVertexColor* ColorElement = Mesh->GetLayer(0)->GetVertexColors();
        if (!ColorElement)
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        int32 ColorIndex = 0;

        switch (ColorElement->GetMappingMode())
        {
        case FbxLayerElement::eByControlPoint:
            ColorIndex = ControlPointIndex;
            break;

        case FbxLayerElement::eByPolygonVertex:
            ColorIndex = PolygonVertexIndex;
            break;

        case FbxLayerElement::eAllSame:
            ColorIndex = 0;
            break;

        default:
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (ColorIndex < 0)
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (ColorElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
        {
            if (ColorIndex >= ColorElement->GetIndexArray().GetCount())
            {
                return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
            }

            ColorIndex = ColorElement->GetIndexArray().GetAt(ColorIndex);
        }

        if (ColorIndex < 0 || ColorIndex >= ColorElement->GetDirectArray().GetCount())
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        const FbxColor C = ColorElement->GetDirectArray().GetAt(ColorIndex);

        return FVector4(static_cast<float>(C.mRed), static_cast<float>(C.mGreen), static_cast<float>(C.mBlue), static_cast<float>(C.mAlpha));
    }

    // FBX tangent layer에서 tangent를 읽고 성공 여부를 반환한다.
    static bool TryReadTangent(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector4& OutTangent)
    {
        if (!Mesh || !Mesh->GetLayer(0))
        {
            return false;
        }

        FbxLayerElementTangent* TangentElement = Mesh->GetLayer(0)->GetTangents();
        if (!TangentElement)
        {
            return false;
        }

        int32 TangentIndex = 0;

        switch (TangentElement->GetMappingMode())
        {
        case FbxLayerElement::eByControlPoint:
            TangentIndex = ControlPointIndex;
            break;

        case FbxLayerElement::eByPolygonVertex:
            TangentIndex = PolygonVertexIndex;
            break;

        case FbxLayerElement::eAllSame:
            TangentIndex = 0;
            break;

        default:
            return false;
        }

        if (TangentIndex < 0)
        {
            return false;
        }

        if (TangentElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
        {
            if (TangentIndex >= TangentElement->GetIndexArray().GetCount())
            {
                return false;
            }

            TangentIndex = TangentElement->GetIndexArray().GetAt(TangentIndex);
        }

        if (TangentIndex < 0 || TangentIndex >= TangentElement->GetDirectArray().GetCount())
        {
            return false;
        }

        const FbxVector4 T = TangentElement->GetDirectArray().GetAt(TangentIndex);

        FVector Tangent(static_cast<float>(T[0]), static_cast<float>(T[1]), static_cast<float>(T[2]));

        Tangent = Tangent.Normalized();

        const float W = (std::fabs(static_cast<float>(T[3])) > 1.0e-6f) ? static_cast<float>(T[3]) : 1.0f;
        OutTangent    = FVector4(Tangent.X, Tangent.Y, Tangent.Z, W);

        return true;
    }

    template <typename LayerElementType>
    // FBX layer element의 mapping/reference mode에 맞춰 Vector4 값을 읽는다.
    static bool TryGetLayerElementVector4(LayerElementType* Element, int32 ControlPointIndex, int32 PolygonVertexIndex, FbxVector4& OutValue)
    {
        if (!Element)
        {
            return false;
        }

        int32 ElementIndex = 0;

        switch (Element->GetMappingMode())
        {
        case FbxLayerElement::eByControlPoint:
            ElementIndex = ControlPointIndex;
            break;

        case FbxLayerElement::eByPolygonVertex:
            ElementIndex = PolygonVertexIndex;
            break;

        case FbxLayerElement::eAllSame:
            ElementIndex = 0;
            break;

        default:
            return false;
        }

        if (ElementIndex < 0)
        {
            return false;
        }

        if (Element->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
        {
            if (ElementIndex >= Element->GetIndexArray().GetCount())
            {
                return false;
            }

            ElementIndex = Element->GetIndexArray().GetAt(ElementIndex);
        }

        if (ElementIndex < 0 || ElementIndex >= Element->GetDirectArray().GetCount())
        {
            return false;
        }

        OutValue = Element->GetDirectArray().GetAt(ElementIndex);
        return true;
    }

    // morph target shape의 normal delta 계산용 target normal을 읽는다.
    static bool TryReadShapeNormal(FbxShape* Shape, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector& OutNormal)
    {
        if (!Shape || !Shape->GetLayer(0))
        {
            return false;
        }

        FbxVector4 Value;
        if (!TryGetLayerElementVector4(Shape->GetLayer(0)->GetNormals(), ControlPointIndex, PolygonVertexIndex, Value))
        {
            return false;
        }

        OutNormal = FVector(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2])).Normalized();
        return true;
    }

    // morph target shape의 tangent delta 계산용 target tangent를 읽는다.
    static bool TryReadShapeTangent(FbxShape* Shape, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector4& OutTangent)
    {
        if (!Shape || !Shape->GetLayer(0))
        {
            return false;
        }

        FbxVector4 Value;
        if (!TryGetLayerElementVector4(Shape->GetLayer(0)->GetTangents(), ControlPointIndex, PolygonVertexIndex, Value))
        {
            return false;
        }

        FVector Tangent(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2]));
        Tangent = Tangent.Normalized();

        const float W = (std::fabs(static_cast<float>(Value[3])) > 1.0e-6f) ? static_cast<float>(Value[3]) : 1.0f;
        OutTangent    = FVector4(Tangent.X, Tangent.Y, Tangent.Z, W);
        return true;
    }

    // 삼각형 position/UV로 tangent fallback을 계산한다.
    static FVector ComputeTriangleTangent(
        const FVector&  P0,
        const FVector&  P1,
        const FVector&  P2,
        const FVector2& UV0,
        const FVector2& UV1,
        const FVector2& UV2
        )
    {
        const FVector Edge1 = P1 - P0;
        const FVector Edge2 = P2 - P0;

        const float DU1 = UV1.X - UV0.X;
        const float DU2 = UV2.X - UV0.X;
        const float DV1 = UV1.Y - UV0.Y;
        const float DV2 = UV2.Y - UV0.Y;

        const float Denom = DU1 * DV2 - DU2 * DV1;

        if (std::fabs(Denom) <= 1e-6f)
        {
            return FVector(1.0f, 0.0f, 0.0f);
        }

        const float R       = 1.0f / Denom;
        FVector     Tangent = (Edge1 * DV2 - Edge2 * DV1) * R;

        return Tangent.Normalized();
    }

    // control point의 bone weight 목록에 동일 bone weight를 병합해 추가한다.
    static void AddBoneWeight(TArray<FImportedBoneWeight>& Weights, int32 BoneIndex, float Weight)
    {
        if (Weight <= 0.0f)
        {
            return;
        }

        for (FImportedBoneWeight& Existing : Weights)
        {
            if (Existing.BoneIndex == BoneIndex)
            {
                Existing.Weight += Weight;
                return;
            }
        }

        FImportedBoneWeight NewWeight;
        NewWeight.BoneIndex = BoneIndex;
        NewWeight.Weight    = Weight;
        Weights.push_back(NewWeight);
    }

    struct FImportedMorphSourceVertex
    {
        FbxMesh* Mesh               = nullptr;
        int32    ControlPointIndex  = -1;
        int32    PolygonIndex       = -1;
        int32    CornerIndex        = -1;
        int32    PolygonVertexIndex = -1;
        uint32   VertexIndex        = 0;
        FMatrix  MeshToReference;
        FMatrix  NormalToReference;
        FVector  BaseNormalInReference;
        FVector4 BaseTangentInReference;
    };

    enum class ESkeletalImportMeshKind : uint8
    {
        Skinned,
        RigidAttached,
        Loose
    };

    struct FSkeletalImportMeshNode
    {
        FbxNode*                MeshNode       = nullptr;
        ESkeletalImportMeshKind Kind           = ESkeletalImportMeshKind::Loose;
        int32                   RigidBoneIndex = -1;
        FbxNode*                RigidBoneNode  = nullptr;
    };

    // skin이 없는 mesh node의 parent chain에서 import skeleton에 포함된 가장 가까운 bone을 찾는다.
    static bool FindNearestParentBoneIndex(
        FbxNode*                     MeshNode,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        FbxNode*&                    OutBoneNode,
        int32&                       OutBoneIndex
        )
    {
        FbxNode* Current = MeshNode ? MeshNode->GetParent() : nullptr;

        while (Current && !IsSceneRootNode(Current))
        {
            auto BoneIt = BoneNodeToIndex.find(Current);
            if (BoneIt != BoneNodeToIndex.end())
            {
                OutBoneNode = Current;
                OutBoneIndex = BoneIt->second;
                return true;
            }

            Current = Current->GetParent();
        }
        
        OutBoneNode = nullptr;
        OutBoneIndex = -1;
        return false;
    }

    // skeletal import 대상 mesh를 skinned / rigid bone attachment / loose mesh로 분류한다.
    static void ClassifySkeletalImportMeshNodes(
        const TArray<FbxNode*>&       MeshNodes,
        const TMap<FbxNode*, int32>&  BoneNodeToIndex,
        TArray<FSkeletalImportMeshNode>& OutImportNodes
        )
    {
        OutImportNodes.clear();

        for (FbxNode* MeshNode : MeshNodes)
        {
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
            if (!Mesh)
            {
                continue;
            }

            FSkeletalImportMeshNode ImportNode;
            ImportNode.MeshNode = MeshNode;

            if (MeshHasSkin(Mesh))
            {
                ImportNode.Kind = ESkeletalImportMeshKind::Skinned;
                OutImportNodes.push_back(ImportNode);
                continue;
            }

            int32    ParentBoneIndex = -1;
            FbxNode* ParentBoneNode  = nullptr;
            if (FindNearestParentBoneIndex(MeshNode, BoneNodeToIndex, ParentBoneNode, ParentBoneIndex))
            {
                ImportNode.Kind           = ESkeletalImportMeshKind::RigidAttached;
                ImportNode.RigidBoneIndex = ParentBoneIndex;
                ImportNode.RigidBoneNode  = ParentBoneNode;
                OutImportNodes.push_back(ImportNode);
                continue;
            }

            ImportNode.Kind = ESkeletalImportMeshKind::Loose;
            OutImportNodes.push_back(ImportNode);
        }
    }

    // position에 행렬의 전체 transform을 적용한다.
    static FVector TransformPositionByMatrix(const FVector& P, const FMatrix& M)
    {
        return P * M;
    }

    // direction vector에 행렬 회전/스케일 성분을 적용하고 정규화한다.
    static FVector TransformDirectionByMatrix(const FVector& V, const FMatrix& M)
    {
        return M.TransformVector(V).Normalized();
    }

    // normal matrix로 normal을 변환하고 정규화한다.
    static FVector TransformNormalByMatrix(const FVector& V, const FMatrix& NormalMatrix)
    {
        return NormalMatrix.TransformVector(V).Normalized();
    }

    // tangent를 normal에 직교하도록 보정한다.
    static FVector OrthogonalizeTangentToNormal(const FVector& Tangent, const FVector& Normal)
    {
        const FVector N = Normal.Normalized();

        FVector T = Tangent - (N * Tangent.Dot(N));

        if (T.IsNearlyZero(1.0e-6f))
        {
            const FVector Candidate = (std::fabs(N.Z) < 0.999f) ? FVector::UpVector : FVector::RightVector;

            T = Candidate - (N * Candidate.Dot(N));
        }

        return T.Normalized();
    }

    // tangent를 reference space로 변환한 뒤 normal에 직교화한다.
    static FVector TransformTangentByMatrix(const FVector& Tangent, const FMatrix& TangentMatrix, const FVector& ReferenceNormal)
    {
        const FVector ReferenceTangent = TransformDirectionByMatrix(Tangent, TangentMatrix);
        return OrthogonalizeTangentToNormal(ReferenceTangent, ReferenceNormal);
    }

    // morph delta처럼 길이를 보존해야 하는 vector를 정규화 없이 변환한다.
    static FVector TransformVectorNoNormalizeByMatrix(const FVector& V, const FMatrix& M)
    {
        return M.TransformVector(V);
    }

    // 행렬의 basis 축을 정규화해 scale이 제거된 회전 행렬을 만든다.
    static FMatrix RemoveScaleFromMatrix(const FMatrix& Matrix)
    {
        FVector XAxis(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
        FVector YAxis(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
        FVector ZAxis(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

        XAxis = XAxis.Normalized();
        YAxis = YAxis.Normalized();
        ZAxis = ZAxis.Normalized();

        FMatrix Result = FMatrix::Identity;

        Result.M[0][0] = XAxis.X;
        Result.M[0][1] = XAxis.Y;
        Result.M[0][2] = XAxis.Z;

        Result.M[1][0] = YAxis.X;
        Result.M[1][1] = YAxis.Y;
        Result.M[1][2] = YAxis.Z;

        Result.M[2][0] = ZAxis.X;
        Result.M[2][1] = ZAxis.Y;
        Result.M[2][2] = ZAxis.Z;

        return Result;
    }

    // local matrix를 animation key의 TRS 값으로 분해한다.
    static FBoneTransformKey MakeBoneTransformKeyFromEngineMatrix(float TimeSeconds, const FMatrix& LocalMatrix)
    {
        FBoneTransformKey Key;
        Key.TimeSeconds = TimeSeconds;
        Key.Translation = LocalMatrix.GetLocation();
        Key.Scale       = LocalMatrix.GetScale();
        Key.Rotation    = RemoveScaleFromMatrix(LocalMatrix).ToQuat().GetNormalized();
        return Key;
    }

    // FBX Scene의 bind pose 목록에서 특정 node의 pose matrix를 찾는다.
    static bool TryGetBindPoseMatrixForNode(FbxScene* Scene, FbxNode* Node, FMatrix& OutPoseMatrix)
    {
        if (!Scene || !Node)
        {
            return false;
        }

        const int32 PoseCount = Scene->GetPoseCount();

        for (int32 PoseIndex = 0; PoseIndex < PoseCount; ++PoseIndex)
        {
            FbxPose* Pose = Scene->GetPose(PoseIndex);
            if (!Pose || !Pose->IsBindPose())
            {
                continue;
            }

            const int32 NodeIndex = Pose->Find(Node);
            if (NodeIndex < 0)
            {
                continue;
            }

            OutPoseMatrix = ToEngineMatrix(Pose->GetMatrix(NodeIndex));
            return true;
        }

        return false;
    }

    // Mesh의 bind matrix를 FBX bind pose 우선, skin cluster matrix fallback으로 얻는다.
    static bool TryGetFirstMeshBindMatrix(FbxScene* Scene, FbxNode* MeshNode, FMatrix& OutMeshBindMatrix)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
        if (!Mesh)
        {
            return false;
        }

        const FMatrix GeometryTransform = ToEngineMatrix(GetNodeGeometryTransform(MeshNode));

        FMatrix PoseMatrix;
        if (TryGetBindPoseMatrixForNode(Scene, MeshNode, PoseMatrix))
        {
            OutMeshBindMatrix = GeometryTransform * PoseMatrix;
            return true;
        }

        const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

        for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
            if (!Skin)
            {
                continue;
            }

            const int32 ClusterCount = Skin->GetClusterCount();

            for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
            {
                FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
                if (!Cluster)
                {
                    continue;
                }

                FbxAMatrix MeshNodeBindFbx;
                Cluster->GetTransformMatrix(MeshNodeBindFbx);

                const FMatrix MeshNodeBind = ToEngineMatrix(MeshNodeBindFbx);

                OutMeshBindMatrix = GeometryTransform * MeshNodeBind;
                return true;
            }
        }

        return false;
    }


    // skin이 없는 rigid attachment mesh의 bind matrix를 얻는다.
    static FMatrix GetRigidMeshBindMatrix(FbxScene* Scene, FbxNode* MeshNode)
    {
        if (!MeshNode)
        {
            return FMatrix::Identity;
        }

        const FMatrix GeometryTransform = ToEngineMatrix(GetNodeGeometryTransform(MeshNode));

        FMatrix PoseMatrix;
        if (TryGetBindPoseMatrixForNode(Scene, MeshNode, PoseMatrix))
        {
            return GeometryTransform * PoseMatrix;
        }

        return GeometryTransform * ToEngineMatrix(MeshNode->EvaluateGlobalTransform());
    }

    // reference LOD node 목록에서 기준 mesh bind matrix를 FBX bind pose 우선으로 찾는다.
    static bool TryGetReferenceMeshBindMatrix(FbxScene* Scene, const TArray<FbxNode*>& SkinnedMeshNodes, FMatrix& OutReferenceMeshBindMatrix)
    {
        for (FbxNode* MeshNode : SkinnedMeshNodes)
        {
            if (TryGetFirstMeshBindMatrix(Scene, MeshNode, OutReferenceMeshBindMatrix))
            {
                return true;
            }
        }

        return false;
    }

    // Mesh의 skin cluster weight만 control point별로 추출하고 import 경고를 기록한다.
    static void ExtractSkinWeightsOnly(
        FbxMesh*                             Mesh,
        const TMap<FbxNode*, int32>&         BoneNodeToIndex,
        TArray<TArray<FImportedBoneWeight>>& OutControlPointWeight,
        FImportBuildContext&                 BuildContext
        )
    {
        OutControlPointWeight.clear();

        if (!Mesh)
        {
            return;
        }

        const int32 ControlPointCount = Mesh->GetControlPointsCount();
        OutControlPointWeight.resize(ControlPointCount);

        const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

        for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
            if (!Skin)
            {
                continue;
            }

            const FbxSkin::EType SkinningType = Skin->GetSkinningType();

            if (SkinningType != FbxSkin::eLinear && SkinningType != FbxSkin::eRigid)
            {
                BuildContext.AddWarning(
                    ESkeletalImportWarningType::UnsupportedSkinningType,
                    "Unsupported FBX skinning type. Imported with linear skinning fallback."
                );
            }

            const int32 ClusterCount = Skin->GetClusterCount();

            for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
            {
                FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
                if (!Cluster || !Cluster->GetLink())
                {
                    continue;
                }

                const FbxCluster::ELinkMode LinkMode = Cluster->GetLinkMode();

                if (LinkMode != FbxCluster::eNormalize && LinkMode != FbxCluster::eTotalOne)
                {
                    BuildContext.AddWarning(
                        ESkeletalImportWarningType::UnsupportedClusterLinkMode,
                        "Unsupported FBX cluster link mode. Additive/associate model is not fully supported."
                    );
                }

                auto BoneIt = BoneNodeToIndex.find(Cluster->GetLink());
                if (BoneIt == BoneNodeToIndex.end())
                {
                    continue;
                }

                const int32 BoneIndex = BoneIt->second;

                const int32*  ControlPointIndices = Cluster->GetControlPointIndices();
                const double* ControlPointWeights = Cluster->GetControlPointWeights();
                const int32   InfluenceCount      = Cluster->GetControlPointIndicesCount();

                for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
                {
                    const int32 ControlPointIndex = ControlPointIndices[InfluenceIndex];
                    const float Weight            = static_cast<float>(ControlPointWeights[InfluenceIndex]);

                    if (ControlPointIndex < 0 || ControlPointIndex >= ControlPointCount)
                    {
                        continue;
                    }

                    AddBoneWeight(OutControlPointWeight[ControlPointIndex], BoneIndex, Weight);
                }
            }
        }
    }

    // scene node global transform을 기준으로 모든 bone bind pose를 reference space에 초기화한다.
    static void InitializeBoneBindPoseFromSceneNodes(const TMap<FbxNode*, int32>& BoneNodeToIndex, const FMatrix& ReferenceMeshBindInverse, FSkeleton& Skeleton)
    {
        for (const auto& Pair : BoneNodeToIndex)
        {
            FbxNode*    BoneNode  = Pair.first;
            const int32 BoneIndex = Pair.second;

            if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
            {
                continue;
            }

            const FMatrix BoneGlobal = ToEngineMatrix(BoneNode->EvaluateGlobalTransform());

            const FMatrix BoneInReferenceMeshSpace = BoneGlobal * ReferenceMeshBindInverse;

            Skeleton.Bones[BoneIndex].GlobalBindPose  = BoneInReferenceMeshSpace;
            Skeleton.Bones[BoneIndex].InverseBindPose = BoneInReferenceMeshSpace.GetInverse();
        }
    }

    // FBX bind pose matrix를 bone bind pose로 우선 적용한다.
    static void ApplyBindPoseFromFbxPose(
        FbxScene*                    Scene,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FMatrix&               ReferenceMeshBindInverse,
        FSkeleton&                   Skeleton,
        TArray<bool>&                InOutAppliedBoneMask,
        FImportBuildContext&         BuildContext
        )
    {
        if (!Scene)
        {
            return;
        }

        bool bFoundAnyBindPose = false;

        for (const auto& Pair : BoneNodeToIndex)
        {
            FbxNode*    BoneNode  = Pair.first;
            const int32 BoneIndex = Pair.second;

            if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
            {
                continue;
            }

            FMatrix BonePoseMatrix;
            if (!TryGetBindPoseMatrixForNode(Scene, BoneNode, BonePoseMatrix))
            {
                continue;
            }

            bFoundAnyBindPose = true;

            const FMatrix BoneInReferenceMeshSpace = BonePoseMatrix * ReferenceMeshBindInverse;

            Skeleton.Bones[BoneIndex].GlobalBindPose  = BoneInReferenceMeshSpace;
            Skeleton.Bones[BoneIndex].InverseBindPose = BoneInReferenceMeshSpace.GetInverse();

            if (BoneIndex < static_cast<int32>(InOutAppliedBoneMask.size()))
            {
                InOutAppliedBoneMask[BoneIndex] = true;
            }
        }

        if (!bFoundAnyBindPose)
        {
            BuildContext.AddWarning(
                ESkeletalImportWarningType::MissingBindPose,
                "No explicit FBX bind pose was found. Falling back to skin cluster bind matrices."
            );
        }
    }

    // skin cluster의 link bind matrix로 bone bind pose를 reference space 기준으로 덮어쓴다.
    static void ApplyBindPoseFromSkinClusters(
        const TArray<FbxNode*>&      SkinnedMeshNodes,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FMatrix&               ReferenceMeshBindInverse,
        FSkeleton&                   Skeleton,
        TArray<bool>*                InOutAppliedBoneMask = nullptr
        )
    {
        for (FbxNode* MeshNode : SkinnedMeshNodes)
        { 
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
            if (!Mesh)
            {
                continue;
            }

            const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

            for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
            {
                FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
                if (!Skin)
                {
                    continue;
                }

                const int32 ClusterCount = Skin->GetClusterCount();

                for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
                {
                    FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
                    if (!Cluster || !Cluster->GetLink())
                    {
                        continue;
                    }

                    auto BoneIt = BoneNodeToIndex.find(Cluster->GetLink());
                    if (BoneIt == BoneNodeToIndex.end())
                    {
                        continue;
                    }

                    const int32 BoneIndex = BoneIt->second;

                    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                    {
                        continue;
                    }

                    if (InOutAppliedBoneMask && BoneIndex < static_cast<int32>(InOutAppliedBoneMask->size()) && (*InOutAppliedBoneMask)[BoneIndex])
                    {
                        continue;
                    }

                    FbxAMatrix LinkBindFbx;
                    Cluster->GetTransformLinkMatrix(LinkBindFbx);

                    const FMatrix LinkBind = ToEngineMatrix(LinkBindFbx);

                    const FMatrix BoneBindInReferenceMeshSpace = LinkBind * ReferenceMeshBindInverse;

                    Skeleton.Bones[BoneIndex].GlobalBindPose  = BoneBindInReferenceMeshSpace;
                    Skeleton.Bones[BoneIndex].InverseBindPose = BoneBindInReferenceMeshSpace.GetInverse();

                    if (InOutAppliedBoneMask && BoneIndex < static_cast<int32>(InOutAppliedBoneMask->size()))
                    {
                        (*InOutAppliedBoneMask)[BoneIndex] = true;
                    }
                }
            }
        }
    }

    struct FPackedBoneWeightStats
    {
        bool  bMissingWeight       = false;
        bool  bOverMaxInfluences   = false;
        bool  bBoneIndexOverflow   = false;
        int32 SourceInfluenceCount = 0;
        float DiscardedWeight      = 0.0f;
    };


    // skin이 없는 rigid attachment mesh vertex를 특정 bone 100% weight로 채운다.
    static void SetRigidBoneWeight(
        int32  BoneIndex,
        uint16 OutBoneIndices[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        float  OutBoneWeights[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        FPackedBoneWeightStats& OutStats
        )
    {
        OutStats = FPackedBoneWeightStats();
        OutStats.SourceInfluenceCount = 1;

        for (int32 i = 0; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++i)
        {
            OutBoneIndices[i] = 0;
            OutBoneWeights[i] = 0.0f;
        }

        if (BoneIndex < 0 || BoneIndex > 65535)
        {
            OutStats.bBoneIndexOverflow = true;
            OutStats.bMissingWeight     = true;
            OutBoneIndices[0]           = 0;
            OutBoneWeights[0]           = 1.0f;
            return;
        }

        OutBoneIndices[0] = static_cast<uint16>(BoneIndex);
        OutBoneWeights[0] = 1.0f;
    }

    // control point weight를 상위 N개 influence로 압축한다.
    // 통계는 dedup 후 실제로 채택된 unique vertex에 대해서만 CommitUniqueVertexImportStats()에서 반영한다.
    static void PackTopBoneWeights(
        const TArray<FImportedBoneWeight>& SourceWeight,
        uint16                             OutBoneIndices[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        float                              OutBoneWeights[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        FPackedBoneWeightStats&            OutStats
        )
    {
        OutStats = FPackedBoneWeightStats();

        for (int32 i = 0; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++i)
        {
            OutBoneIndices[i] = 0;
            OutBoneWeights[i] = 0.0f;
        }

        if (SourceWeight.empty())
        {
            OutBoneIndices[0]       = 0;
            OutBoneWeights[0]       = 1.0f;
            OutStats.bMissingWeight = true;
            return;
        }

        TArray<FImportedBoneWeight> SortedWeights = SourceWeight;
        std::sort(
            SortedWeights.begin(),
            SortedWeights.end(),
            [](const FImportedBoneWeight& A, const FImportedBoneWeight& B)
            {
                return A.Weight > B.Weight;
            }
        );

        OutStats.SourceInfluenceCount = static_cast<int32>(SortedWeights.size());

        if (static_cast<int32>(SortedWeights.size()) > MAX_SKELETAL_MESH_BONE_INFLUENCES)
        {
            OutStats.bOverMaxInfluences = true;

            for (int32 i = MAX_SKELETAL_MESH_BONE_INFLUENCES; i < static_cast<int32>(SortedWeights.size()); ++i)
            {
                OutStats.DiscardedWeight += SortedWeights[i].Weight;
            }
        }

        const int32 Count = static_cast<int32>((std::min<std::size_t>)(static_cast<std::size_t>(MAX_SKELETAL_MESH_BONE_INFLUENCES), SortedWeights.size()));

        float Sum = 0.0f;

        for (int32 i = 0; i < Count; ++i)
        {
            if (SortedWeights[i].BoneIndex < 0 || SortedWeights[i].BoneIndex > 65535)
            {
                OutStats.bBoneIndexOverflow = true;
                continue;
            }

            OutBoneIndices[i] = static_cast<uint16>(SortedWeights[i].BoneIndex);
            OutBoneWeights[i] = SortedWeights[i].Weight;
            Sum               += OutBoneWeights[i];
        }

        if (Sum <= 1e-6f)
        {
            OutBoneIndices[0] = 0;
            OutBoneWeights[0] = 1.0f;

            for (int32 i = 1; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++i)
            {
                OutBoneIndices[i] = 0;
                OutBoneWeights[i] = 0.0f;
            }

            OutStats.bMissingWeight = true;
            return;
        }

        for (int32 i = 0; i < Count; ++i)
        {
            OutBoneWeights[i] /= Sum;
        }
    }

    static void CommitUniqueVertexImportStats(
        FImportBuildContext&          BuildContext,
        bool                          bGeneratedNormal,
        bool                          bGeneratedTangent,
        bool                          bMissingUV,
        const FPackedBoneWeightStats& BoneWeightStats
        )
    {
        BuildContext.Summary.VertexCount++;

        if (bGeneratedNormal)
        {
            BuildContext.Summary.GeneratedNormalCount++;
        }

        if (bGeneratedTangent)
        {
            BuildContext.Summary.GeneratedTangentCount++;
        }

        if (bMissingUV)
        {
            BuildContext.Summary.MissingUVCount++;
        }

        if (BoneWeightStats.bMissingWeight)
        {
            BuildContext.Summary.MissingWeightVertexCount++;
        }

        BuildContext.Summary.MaxInfluenceCount = (std::max)(BuildContext.Summary.MaxInfluenceCount, BoneWeightStats.SourceInfluenceCount);

        if (BoneWeightStats.bOverMaxInfluences)
        {
            BuildContext.Summary.VertexCountOverMaxInfluences++;
            BuildContext.Summary.TotalDiscardedWeight += BoneWeightStats.DiscardedWeight;
        }

        if (BoneWeightStats.bBoneIndexOverflow)
        {
            BuildContext.AddWarning(ESkeletalImportWarningType::BoneIndexOverflow, "Bone index is outside uint16 range.");
        }
    }

    struct FImportedSectionBuild
    {
        int32          MaterialIndex = 0;
        TArray<uint32> Indices;
    };

    // material index에 해당하는 임시 section build 데이터를 찾거나 새로 만든다.
    static FImportedSectionBuild* FindOrAddImportedSection(TArray<FImportedSectionBuild>& Sections, int32 MaterialIndex)
    {
        for (FImportedSectionBuild& Section : Sections)
        {
            if (Section.MaterialIndex == MaterialIndex)
            {
                return &Section;
            }
        }

        FImportedSectionBuild NewSectionBuild;
        NewSectionBuild.MaterialIndex = MaterialIndex;
        Sections.push_back(NewSectionBuild);
        return &Sections.back();
    }

    // FBX material layer에서 polygon의 material index를 읽는다.
    static int32 GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
    {
        if (!Mesh || !Mesh->GetLayer(0))
        {
            return 0;
        }

        FbxLayerElementMaterial* MaterialElement = Mesh->GetLayer(0)->GetMaterials();
        if (!MaterialElement)
        {
            return 0;
        }

        if (MaterialElement->GetMappingMode() == FbxLayerElement::eByPolygon)
        {
            if (MaterialElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect || MaterialElement->GetReferenceMode() == FbxLayerElement::eIndex)
            {
                if (PolygonIndex >= 0 && PolygonIndex < MaterialElement->GetIndexArray().GetCount())
                {
                    return MaterialElement->GetIndexArray().GetAt(PolygonIndex);
                }
            }
            return 0;
        }
        if (MaterialElement->GetMappingMode() == FbxLayerElement::eAllSame)
        {
            if (MaterialElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect || MaterialElement->GetReferenceMode() == FbxLayerElement::eIndex)
            {
                if (MaterialElement->GetIndexArray().GetCount() > 0)
                {
                    return MaterialElement->GetIndexArray().GetAt(0);
                }
            }

            return 0;
        }

        return 0;
    }

    // FBX material을 외부 texture path 기반 .mat 파일로 변환하고 material slot에 연결한다.
    static int32 FindOrAddFbxMaterial(
        FbxSurfaceMaterial*      FbxMaterial,
        const FString&           SourceFbxPath,
        TArray<FStaticMaterial>& OutMaterials,
        FImportBuildContext&     BuildContext
        )
    {
        const FString SlotName = FbxMaterial ? FbxMaterial->GetName() : "None";

        for (int32 i = 0; i < static_cast<int32>(OutMaterials.size()); ++i)
        {
            if (OutMaterials[i].MaterialSlotName == SlotName)
            {
                return i;
            }
        }

        FString MaterialPath = "None";

        if (FbxMaterial)
        {
            const FFbxImportedMaterialInfo MaterialInfo = ExtractFbxMaterialInfo(FbxMaterial, SourceFbxPath);
            MaterialPath                                = ConvertFbxMaterialInfoToMat(MaterialInfo, SourceFbxPath, BuildContext);
        }

        FStaticMaterial NewMaterial;
        NewMaterial.MaterialSlotName  = SlotName;
        NewMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
        OutMaterials.push_back(NewMaterial);
        return static_cast<int32>(OutMaterials.size()) - 1;
    }

    // 임시 section별 index 목록을 최종 index buffer와 section 배열로 병합한다.
    static void BuildFinalImportedSections(
        const TArray<FImportedSectionBuild>& SectionBuilds,
        const TArray<FStaticMaterial>&       Materials,
        TArray<uint32>&                      OutIndices,
        TArray<FStaticMeshSection>&          OutSections
        )
    {
        OutSections.clear();
        OutIndices.clear();

        for (const FImportedSectionBuild& Build : SectionBuilds)
        {
            if (Build.Indices.empty())
            {
                continue;
            }

            FStaticMeshSection Section;
            Section.MaterialIndex    = Build.MaterialIndex;
            Section.MaterialSlotName = Materials[Build.MaterialIndex].MaterialSlotName;
            Section.FirstIndex       = static_cast<uint32>(OutIndices.size());
            Section.NumTriangles     = static_cast<uint32>(Build.Indices.size() / 3);

            for (uint32 Index : Build.Indices)
            {
                OutIndices.push_back(Index);
            }

            OutSections.push_back(Section);
        }
    }

    // reference-space global bind pose로부터 parent 기준 local bind pose를 재계산한다.
    static void RecomputeLocalBindPose(FSkeleton& Skeleton)
    {
        for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
        {
            FBoneInfo& Bone = Skeleton.Bones[BoneIndex];

            if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(Skeleton.Bones.size()))
            {
                const FMatrix ParentGlobal = Skeleton.Bones[Bone.ParentIndex].GlobalBindPose;
                Bone.LocalBindPose         = Bone.GlobalBindPose * ParentGlobal.GetInverse();
            }
            else
            {
                Bone.LocalBindPose = Bone.GlobalBindPose;
            }
        }
        Skeleton.RebuildChildren();
    }

    // FBX scene의 axis system과 unit을 엔진 기준으로 변환한다.
    static void NormalizeFbxScene(FbxScene* Scene)
    {
        if (!Scene)
        {
            return;
        }

        FbxAxisSystem EngineAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);

        const FbxAxisSystem SceneAxisSystem = Scene->GetGlobalSettings().GetAxisSystem();

        if (SceneAxisSystem != EngineAxisSystem)
        {
            EngineAxisSystem.ConvertScene(Scene);
        }

        const FbxSystemUnit EngineUnit = FbxSystemUnit::cm;
        const FbxSystemUnit SceneUnit  = Scene->GetGlobalSettings().GetSystemUnit();

        if (SceneUnit != EngineUnit)
        {
            EngineUnit.ConvertScene(Scene);
        }
    }

    // FBX animation stack을 샘플링해 bone별 TRS animation track으로 변환한다.
    static void ImportAnimations(
        FbxScene*                       Scene,
        const TMap<FbxNode*, int32>&    BoneNodeToIndex,
        const FMatrix&                  ReferenceMeshBindInverse,
        const FSkeleton&                Skeleton,
        TArray<FSkeletalAnimationClip>& OutAnimations
        )
    {
        OutAnimations.clear();

        if (!Scene)
        {
            return;
        }

        const int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
        if (AnimStackCount <= 0)
        {
            return;
        }

        const float SampleRate = 30.0f;

        for (int32 StackIndex = 0; StackIndex < AnimStackCount; ++StackIndex)
        {
            FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
            if (!AnimStack)
            {
                continue;
            }

            Scene->SetCurrentAnimationStack(AnimStack);

            FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();

            const double StartSeconds = TimeSpan.GetStart().GetSecondDouble();
            const double EndSeconds   = TimeSpan.GetStop().GetSecondDouble();

            if (EndSeconds <= StartSeconds)
            {
                continue;
            }

            FSkeletalAnimationClip Clip;
            Clip.Name            = AnimStack->GetName();
            Clip.DurationSeconds = static_cast<float>(EndSeconds - StartSeconds);
            Clip.SampleRate      = SampleRate;

            Clip.Tracks.resize(Skeleton.Bones.size());

            for (const auto& Pair : BoneNodeToIndex)
            {
                const int32 BoneIndex = Pair.second;

                if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                {
                    continue;
                }

                FBoneAnimationTrack& Track = Clip.Tracks[BoneIndex];
                Track.BoneIndex            = BoneIndex;
                Track.BoneName             = Skeleton.Bones[BoneIndex].Name;
            }

            const double DurationSeconds = static_cast<double>(Clip.DurationSeconds);
            const int32  WholeFrameCount = static_cast<int32>(std::floor(DurationSeconds * static_cast<double>(SampleRate) + 1.0e-6));

            // 지정 시간의 bone global transform을 reference space local key로 샘플링한다.
            auto AddAnimationKeysAtTime = [&](double LocalSeconds)
            {
                if (LocalSeconds < 0.0)
                {
                    LocalSeconds = 0.0;
                }
                else if (LocalSeconds > DurationSeconds)
                {
                    LocalSeconds = DurationSeconds;
                }

                const double AbsoluteSeconds = StartSeconds + LocalSeconds;

                FbxTime Time;
                Time.SetSecondDouble(AbsoluteSeconds);

                TArray<FMatrix> GlobalInReference;
                GlobalInReference.resize(Skeleton.Bones.size());

                for (const auto& Pair : BoneNodeToIndex)
                {
                    FbxNode*    BoneNode  = Pair.first;
                    const int32 BoneIndex = Pair.second;

                    if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                    {
                        continue;
                    }

                    const FMatrix BoneGlobal     = ToEngineMatrix(BoneNode->EvaluateGlobalTransform(Time));
                    GlobalInReference[BoneIndex] = BoneGlobal * ReferenceMeshBindInverse;
                }

                for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
                {
                    if (BoneIndex >= static_cast<int32>(Clip.Tracks.size()))
                    {
                        continue;
                    }

                    FMatrix     LocalMatrix = GlobalInReference[BoneIndex];
                    const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

                    if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(Skeleton.Bones.size()))
                    {
                        LocalMatrix = GlobalInReference[BoneIndex] * GlobalInReference[ParentIndex].GetInverse();
                    }

                    FBoneTransformKey Key = MakeBoneTransformKeyFromEngineMatrix(static_cast<float>(LocalSeconds), LocalMatrix);
                    Clip.Tracks[BoneIndex].Keys.push_back(Key);
                }
            };

            for (int32 FrameIndex = 0; FrameIndex <= WholeFrameCount; ++FrameIndex)
            {
                AddAnimationKeysAtTime(static_cast<double>(FrameIndex) / static_cast<double>(SampleRate));
            }

            const double LastWholeFrameSeconds = static_cast<double>(WholeFrameCount) / static_cast<double>(SampleRate);
            if (DurationSeconds - LastWholeFrameSeconds > 1.0e-4)
            {
                AddAnimationKeysAtTime(DurationSeconds);
            }

            OutAnimations.push_back(std::move(Clip));
        }
    }

    // FBX blend shape을 최종 vertex index 기준 morph target delta로 변환한다.
    static void ImportMorphTargets(const TArray<TArray<FImportedMorphSourceVertex>>& MorphSourcesByLOD, TArray<FMorphTarget>& OutMorphTargets)
    {
        OutMorphTargets.clear();

        TMap<FString, int32> MorphNameToIndex;

        for (int32 LODIndex = 0; LODIndex < static_cast<int32>(MorphSourcesByLOD.size()); ++LODIndex)
        {
            const TArray<FImportedMorphSourceVertex>& Sources = MorphSourcesByLOD[LODIndex];

            TMap<FbxMesh*, TArray<const FImportedMorphSourceVertex*>> SourcesByMesh;

            for (const FImportedMorphSourceVertex& Source : Sources)
            {
                if (Source.Mesh)
                {
                    SourcesByMesh[Source.Mesh].push_back(&Source);
                }
            }

            for (const auto& MeshPair : SourcesByMesh)
            {
                FbxMesh*                                         Mesh        = MeshPair.first;
                const TArray<const FImportedMorphSourceVertex*>& MeshSources = MeshPair.second;

                if (!Mesh)
                {
                    continue;
                }

                const int32 BlendShapeCount = Mesh->GetDeformerCount(FbxDeformer::eBlendShape);

                for (int32 BlendShapeIndex = 0; BlendShapeIndex < BlendShapeCount; ++BlendShapeIndex)
                {
                    FbxBlendShape* BlendShape = static_cast<FbxBlendShape*>(Mesh->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape));

                    if (!BlendShape)
                    {
                        continue;
                    }

                    const int32 ChannelCount = BlendShape->GetBlendShapeChannelCount();

                    for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
                    {
                        FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

                        if (!Channel)
                        {
                            continue;
                        }

                        const int32 TargetShapeCount = Channel->GetTargetShapeCount();

                        if (TargetShapeCount <= 0)
                        {
                            continue;
                        }

                        FbxShape* Shape = Channel->GetTargetShape(TargetShapeCount - 1);

                        if (!Shape)
                        {
                            continue;
                        }

                        FString MorphName = Channel->GetName();

                        if (MorphName.empty())
                        {
                            MorphName = Shape->GetName();
                        }

                        int32 MorphIndex = -1;

                        auto Existing = MorphNameToIndex.find(MorphName);
                        if (Existing != MorphNameToIndex.end())
                        {
                            MorphIndex = Existing->second;
                        }
                        else
                        {
                            FMorphTarget NewMorph;
                            NewMorph.Name = MorphName;
                            NewMorph.LODModels.resize(MorphSourcesByLOD.size());

                            OutMorphTargets.push_back(std::move(NewMorph));

                            MorphIndex                  = static_cast<int32>(OutMorphTargets.size()) - 1;
                            MorphNameToIndex[MorphName] = MorphIndex;
                        }

                        FMorphTargetLOD& MorphLOD = OutMorphTargets[MorphIndex].LODModels[LODIndex];

                        FbxVector4* BaseControlPoints   = Mesh->GetControlPoints();
                        FbxVector4* TargetControlPoints = Shape->GetControlPoints();

                        if (!BaseControlPoints || !TargetControlPoints)
                        {
                            continue;
                        }

                        const int32 ControlPointCount = Mesh->GetControlPointsCount();

                        for (const FImportedMorphSourceVertex* Source : MeshSources)
                        {
                            if (!Source)
                            {
                                continue;
                            }

                            const int32 CPIndex = Source->ControlPointIndex;

                            if (CPIndex < 0 || CPIndex >= ControlPointCount)
                            {
                                continue;
                            }

                            const FbxVector4 BaseP   = BaseControlPoints[CPIndex];
                            const FbxVector4 TargetP = TargetControlPoints[CPIndex];

                            FVector LocalDelta(
                                static_cast<float>(TargetP[0] - BaseP[0]),
                                static_cast<float>(TargetP[1] - BaseP[1]),
                                static_cast<float>(TargetP[2] - BaseP[2])
                            );

                            FMorphTargetDelta Delta;
                            Delta.VertexIndex   = Source->VertexIndex;
                            Delta.PositionDelta = TransformVectorNoNormalizeByMatrix(LocalDelta, Source->MeshToReference);
                            Delta.NormalDelta   = FVector(0.0f, 0.0f, 0.0f);
                            Delta.TangentDelta  = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

                            FVector TargetNormalInReference = Source->BaseNormalInReference;

                            FVector TargetLocalNormal;
                            if (TryReadShapeNormal(Shape, CPIndex, Source->PolygonVertexIndex, TargetLocalNormal))
                            {
                                TargetNormalInReference = TransformNormalByMatrix(TargetLocalNormal, Source->NormalToReference);
                                Delta.NormalDelta       = TargetNormalInReference - Source->BaseNormalInReference;
                            }

                            FVector4 TargetLocalTangent;
                            if (TryReadShapeTangent(Shape, CPIndex, Source->PolygonVertexIndex, TargetLocalTangent))
                            {
                                const FVector TargetTangentInReference = TransformTangentByMatrix(
                                    FVector(TargetLocalTangent.X, TargetLocalTangent.Y, TargetLocalTangent.Z),
                                    Source->MeshToReference,
                                    TargetNormalInReference
                                );
                                Delta.TangentDelta = FVector4(
                                    TargetTangentInReference.X - Source->BaseTangentInReference.X,
                                    TargetTangentInReference.Y - Source->BaseTangentInReference.Y,
                                    TargetTangentInReference.Z - Source->BaseTangentInReference.Z,
                                    TargetLocalTangent.W - Source->BaseTangentInReference.W
                                );
                            }

                            if (IsNearlyZeroVector(Delta.PositionDelta) && IsNearlyZeroVector(Delta.NormalDelta) && IsNearlyZeroVector4(Delta.TangentDelta))
                            {
                                continue;
                            }

                            MorphLOD.Deltas.push_back(Delta);
                        }
                    }
                }
            }
        }
    }

    // bind pose skinning 결과가 원본 vertex와 얼마나 다른지 최대 오차를 계산한다.
    static float ValidateBindPoseSkinningError(const FSkeletalMeshLOD& LOD, const FSkeleton& Skeleton)
    {
        float MaxError = 0.0f;

        for (const FSkeletalVertex& Src : LOD.Vertices)
        {
            FVector SkinnedPos(0.0f, 0.0f, 0.0f);

            for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
            {
                const uint16 BoneIndex = Src.BoneIndices[InfluenceIndex];
                const float  Weight    = Src.BoneWeights[InfluenceIndex];

                if (Weight <= 0.0f)
                {
                    continue;
                }

                if (BoneIndex >= Skeleton.Bones.size())
                {
                    continue;
                }

                const FMatrix SkinningMatrix = Skeleton.Bones[BoneIndex].InverseBindPose * Skeleton.Bones[BoneIndex].GlobalBindPose;

                SkinnedPos += (Src.Pos * SkinningMatrix) * Weight;
            }

            const float Error = (SkinnedPos - Src.Pos).Length();
            MaxError          = (std::max)(MaxError, Error);
        }

        return MaxError;
    }
}

// LOD에 속한 skeletal import mesh node들을 하나의 FSkeletalMeshLOD로 빌드한다.
static bool BuildSkeletalMeshLODFromNodes(
    FbxScene*                           Scene,
    const FString&                      SourcePath,
    const TArray<FSkeletalImportMeshNode>& MeshNodes,
    const TMap<FbxNode*, int32>&        BoneNodeToIndex,
    const FMatrix&                      ReferenceMeshBindInverse,
    const FSkeleton& Skeleton,
    TArray<FStaticMaterial>&            OutMaterials,
    FSkeletalMeshLOD&                   OutLOD,
    TArray<FImportedMorphSourceVertex>* OutMorphSources,
    FImportBuildContext&                BuildContext
    )
{
    OutLOD = FSkeletalMeshLOD();

    if (OutMorphSources)
    {
        OutMorphSources->clear();
    }

    TArray<FImportedSectionBuild> SectionBuilds;

    std::unordered_map<FSkeletalVertexDedupKey, uint32, FSkeletalVertexDedupKeyHasher> VertexToIndex;

    for (const FSkeletalImportMeshNode& ImportNode : MeshNodes)
    {
        FbxNode* MeshNode = ImportNode.MeshNode;
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (!Mesh || ImportNode.Kind == ESkeletalImportMeshKind::Loose)
        {
            continue;
        }

        FMatrix MeshToReference;
        
        if (ImportNode.Kind == ESkeletalImportMeshKind::Skinned)
        {
            FMatrix MeshBind;
            if (!TryGetFirstMeshBindMatrix(Scene, MeshNode, MeshBind))
            {
                continue;
            }
            MeshToReference = MeshBind * ReferenceMeshBindInverse;
        }
        else if (ImportNode.Kind == ESkeletalImportMeshKind::RigidAttached)
        {
            if (!ImportNode.RigidBoneNode || ImportNode.RigidBoneIndex < 0 || ImportNode.RigidBoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
            {
                continue;
            }
            
            const FMatrix MeshGlobalScene = ToEngineMatrix(GetNodeGeometryTransform(MeshNode)) * ToEngineMatrix(MeshNode->EvaluateGlobalTransform());
            const FMatrix BoneGlobalScene = ToEngineMatrix(ImportNode.RigidBoneNode->EvaluateGlobalTransform());
            const FMatrix MeshRelativeToBone = MeshGlobalScene * BoneGlobalScene.GetInverse();
            MeshToReference = MeshRelativeToBone * Skeleton.Bones[ImportNode.RigidBoneIndex].GlobalBindPose;
        }
        

        const FMatrix NormalToReference = MeshToReference.GetInverse().GetTransposed();

        TArray<TArray<FImportedBoneWeight>> ControlPointWeight;
        if (ImportNode.Kind == ESkeletalImportMeshKind::Skinned)
        {
            ExtractSkinWeightsOnly(Mesh, BoneNodeToIndex, ControlPointWeight, BuildContext);
        }

        int32 PolygonVertexIndex = 0;

        for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

            if (PolygonSize != 3)
            {
                PolygonVertexIndex += PolygonSize;
                continue;
            }

            const int32 LocalMaterialIndex = GetPolygonMaterialIndex(Mesh, PolygonIndex);

            FbxSurfaceMaterial* FbxMat = nullptr;

            if (LocalMaterialIndex >= 0 && LocalMaterialIndex < MeshNode->GetMaterialCount())
            {
                FbxMat = MeshNode->GetMaterial(LocalMaterialIndex);
            }

            const int32 MaterialIndex = FindOrAddFbxMaterial(FbxMat, SourcePath, OutMaterials, BuildContext);

            FImportedSectionBuild* SectionBuild = FindOrAddImportedSection(SectionBuilds, MaterialIndex);

            int32    ControlPointIndices[3] = {};
            FVector  Positions[3];
            FVector2 UV0[3];

            for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
            {
                ControlPointIndices[CornerIndex] = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);

                const FVector LocalPosition = ReadPosition(Mesh, ControlPointIndices[CornerIndex]);

                Positions[CornerIndex] = TransformPositionByMatrix(LocalPosition, MeshToReference);

                UV0[CornerIndex] = ReadUVByChannel(Mesh, PolygonIndex, CornerIndex, 0);
            }

            const FVector FallbackNormal  = ComputeTriangleNormal(Positions[0], Positions[1], Positions[2]);
            const FVector FallbackTangent = ComputeTriangleTangent(Positions[0], Positions[1], Positions[2], UV0[0], UV0[1], UV0[2]);

            for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
            {
                const int32 ControlPointIndex = ControlPointIndices[CornerIndex];

                const int32 CurrentPolygonVertexIndex = PolygonVertexIndex + CornerIndex;

                FSkeletalVertex Vertex;

                bool                   bGeneratedNormal  = false;
                bool                   bGeneratedTangent = false;
                bool                   bMissingUV        = false;
                FPackedBoneWeightStats BoneWeightStats;

                Vertex.Pos = Positions[CornerIndex];

                FVector LocalNormal;
                if (TryReadNormal(Mesh, PolygonIndex, CornerIndex, LocalNormal))
                {
                    Vertex.Normal = TransformNormalByMatrix(LocalNormal, NormalToReference);
                }
                else
                {
                    Vertex.Normal    = FallbackNormal;
                    bGeneratedNormal = true;
                }

                const int32 RawUVSetCount = GetUVSetCount(Mesh);

                if (RawUVSetCount <= 0)
                {
                    bMissingUV = true;
                }

                const int32 UVCount = static_cast<int32>((std::min<std::size_t>)(
                    static_cast<std::size_t>(MAX_SKELETAL_MESH_UV_CHANNELS),
                    static_cast<std::size_t>(RawUVSetCount)
                ));

                Vertex.NumUVs = static_cast<uint8>(UVCount > 0 ? UVCount : 1);

                for (int32 UVIndex = 0; UVIndex < static_cast<int32>(Vertex.NumUVs); ++UVIndex)
                {
                    Vertex.UV[UVIndex] = ReadUVByChannel(Mesh, PolygonIndex, CornerIndex, UVIndex);
                }

                Vertex.Color = ReadVertexColor(Mesh, ControlPointIndex, CurrentPolygonVertexIndex);

                FVector4 ImportedTangent;
                if (TryReadTangent(Mesh, ControlPointIndex, CurrentPolygonVertexIndex, ImportedTangent))
                {
                    const FVector T = TransformTangentByMatrix(
                        FVector(ImportedTangent.X, ImportedTangent.Y, ImportedTangent.Z),
                        MeshToReference,
                        Vertex.Normal
                    );

                    Vertex.Tangent = FVector4(T.X, T.Y, T.Z, ImportedTangent.W);
                }
                else
                {
                    const FVector T = OrthogonalizeTangentToNormal(FallbackTangent, Vertex.Normal);

                    Vertex.Tangent    = FVector4(T.X, T.Y, T.Z, 1.0f);
                    bGeneratedTangent = true;
                }

                if (ImportNode.Kind == ESkeletalImportMeshKind::RigidAttached)
                {
                    SetRigidBoneWeight(ImportNode.RigidBoneIndex, Vertex.BoneIndices, Vertex.BoneWeights, BoneWeightStats);
                }
                else if (ControlPointIndex >= 0 && ControlPointIndex < static_cast<int32>(ControlPointWeight.size()))
                {
                    PackTopBoneWeights(ControlPointWeight[ControlPointIndex], Vertex.BoneIndices, Vertex.BoneWeights, BoneWeightStats);
                }
                else
                {
                    TArray<FImportedBoneWeight> EmptyWeight;
                    PackTopBoneWeights(EmptyWeight, Vertex.BoneIndices, Vertex.BoneWeights, BoneWeightStats);
                }

                BuildContext.Summary.CandidateVertexCount++;

                bool bAddedNewVertex = false;

                const uint32 VertexIndex = FindOrAddSkeletalVertex(
                    Vertex,
                    Mesh,
                    ControlPointIndex,
                    MaterialIndex,
                    VertexToIndex,
                    OutLOD.Vertices,
                    bAddedNewVertex
                );

                if (bAddedNewVertex)
                {
                    CommitUniqueVertexImportStats(BuildContext, bGeneratedNormal, bGeneratedTangent, bMissingUV, BoneWeightStats);
                }
                else
                {
                    BuildContext.Summary.DeduplicatedVertexCount++;
                }

                SectionBuild->Indices.push_back(VertexIndex);

                if (OutMorphSources && bAddedNewVertex)
                {
                    FImportedMorphSourceVertex Source;
                    Source.Mesh                   = Mesh;
                    Source.ControlPointIndex      = ControlPointIndex;
                    Source.PolygonIndex           = PolygonIndex;
                    Source.CornerIndex            = CornerIndex;
                    Source.PolygonVertexIndex     = CurrentPolygonVertexIndex;
                    Source.VertexIndex            = VertexIndex;
                    Source.MeshToReference        = MeshToReference;
                    Source.NormalToReference      = NormalToReference;
                    Source.BaseNormalInReference  = Vertex.Normal;
                    Source.BaseTangentInReference = Vertex.Tangent;

                    OutMorphSources->push_back(Source);
                }
            }

            BuildContext.Summary.TriangleCount++;
            PolygonVertexIndex += PolygonSize;
        }
    }

    if (OutLOD.Vertices.empty() || SectionBuilds.empty())
    {
        return false;
    }

    BuildFinalImportedSections(SectionBuilds, OutMaterials, OutLOD.Indices, OutLOD.Sections);

    if (OutLOD.Indices.empty() || OutLOD.Sections.empty())
    {
        return false;
    }

    OutLOD.CacheBounds();

    return true;
}

struct FFbxMeshImportSpace
{
    FMatrix VertexTransform = FMatrix::Identity;
    FMatrix NormalTransform = FMatrix::Identity;

    static FFbxMeshImportSpace FromStaticMeshNode(FbxNode* MeshNode)
    {
        FFbxMeshImportSpace Result;

        if (!MeshNode)
        {
            return Result;
        }

        const FMatrix GeometryTransform = ToEngineMatrix(GetNodeGeometryTransform(MeshNode));
        const FMatrix GlobalTransform   = ToEngineMatrix(MeshNode->EvaluateGlobalTransform());

        Result.VertexTransform = GeometryTransform * GlobalTransform;
        Result.NormalTransform = Result.VertexTransform.GetInverse().GetTransposed();
        return Result;
    }
};

struct FFbxTriangleSample
{
    int32    ControlPointIndices[3] = { -1, -1, -1 };
    FVector  Positions[3];
    FVector2 UV0[3];
    FVector  FallbackNormal;
    FVector  FallbackTangent;
};

static FbxSurfaceMaterial* ResolvePolygonFbxMaterial(FbxNode* MeshNode, FbxMesh* Mesh, int32 PolygonIndex)
{
    if (!MeshNode || !Mesh)
    {
        return nullptr;
    }

    const int32 LocalMaterialIndex = GetPolygonMaterialIndex(Mesh, PolygonIndex);
    if (LocalMaterialIndex < 0 || LocalMaterialIndex >= MeshNode->GetMaterialCount())
    {
        return nullptr;
    }

    return MeshNode->GetMaterial(LocalMaterialIndex);
}

static bool ReadTriangleSample(FbxMesh* Mesh, int32 PolygonIndex, const FFbxMeshImportSpace& ImportSpace, FFbxTriangleSample& OutTriangle)
{
    if (!Mesh || Mesh->GetPolygonSize(PolygonIndex) != 3)
    {
        return false;
    }

    for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
    {
        const int32 ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
        OutTriangle.ControlPointIndices[CornerIndex] = ControlPointIndex;

        const FVector LocalPosition = ReadPosition(Mesh, ControlPointIndex);
        OutTriangle.Positions[CornerIndex] = TransformPositionByMatrix(LocalPosition, ImportSpace.VertexTransform);
        OutTriangle.UV0[CornerIndex]       = ReadUVByChannel(Mesh, PolygonIndex, CornerIndex, 0);
    }

    OutTriangle.FallbackNormal = ComputeTriangleNormal(
        OutTriangle.Positions[0],
        OutTriangle.Positions[1],
        OutTriangle.Positions[2]
    );

    OutTriangle.FallbackTangent = ComputeTriangleTangent(
        OutTriangle.Positions[0],
        OutTriangle.Positions[1],
        OutTriangle.Positions[2],
        OutTriangle.UV0[0],
        OutTriangle.UV0[1],
        OutTriangle.UV0[2]
    );

    return true;
}

class FFbxStaticMeshBuilder
{
public:
    FFbxStaticMeshBuilder(
        const FString&           InSourcePath,
        TArray<FStaticMaterial>& InOutMaterials,
        FStaticMesh&             InOutMesh,
        FImportBuildContext&     InBuildContext
        )
        : SourcePath(InSourcePath)
        , Materials(InOutMaterials)
        , MeshAsset(InOutMesh)
        , BuildContext(InBuildContext)
    {
    }

    bool Build(const TArray<FbxNode*>& MeshNodes)
    {
        ResetOutput();

        for (FbxNode* MeshNode : MeshNodes)
        {
            AppendMeshNode(MeshNode);
        }

        return Finalize();
    }

private:
    const FString& SourcePath;
    TArray<FStaticMaterial>& Materials;
    FStaticMesh& MeshAsset;
    FImportBuildContext& BuildContext;

    TArray<FImportedSectionBuild> SectionBuilds;
    std::unordered_map<FStaticVertexDedupKey, uint32, FStaticVertexDedupKeyHasher> VertexToIndex;

    void ResetOutput()
    {
        MeshAsset.Vertices.clear();
        MeshAsset.Indices.clear();
        MeshAsset.Sections.clear();
        SectionBuilds.clear();
        VertexToIndex.clear();
    }

    void AppendMeshNode(FbxNode* MeshNode)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
        if (!Mesh)
        {
            return;
        }

        const FFbxMeshImportSpace ImportSpace = FFbxMeshImportSpace::FromStaticMeshNode(MeshNode);

        int32 PolygonVertexIndex = 0;

        for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

            if (PolygonSize == 3)
            {
                AppendTriangle(MeshNode, Mesh, PolygonIndex, PolygonVertexIndex, ImportSpace);
            }

            PolygonVertexIndex += PolygonSize;
        }
    }

    void AppendTriangle(
        FbxNode*                    MeshNode,
        FbxMesh*                    Mesh,
        int32                       PolygonIndex,
        int32                       PolygonVertexStartIndex,
        const FFbxMeshImportSpace&  ImportSpace
        )
    {
        FFbxTriangleSample Triangle;
        if (!ReadTriangleSample(Mesh, PolygonIndex, ImportSpace, Triangle))
        {
            return;
        }

        const int32 MaterialIndex = ResolveMaterialIndex(MeshNode, Mesh, PolygonIndex);
        FImportedSectionBuild* SectionBuild = FindOrAddImportedSection(SectionBuilds, MaterialIndex);
        if (!SectionBuild)
        {
            return;
        }

        for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
        {
            const uint32 VertexIndex = AddCornerVertex(
                Mesh,
                PolygonIndex,
                CornerIndex,
                PolygonVertexStartIndex + CornerIndex,
                Triangle,
                ImportSpace
            );

            SectionBuild->Indices.push_back(VertexIndex);
        }
    }

    int32 ResolveMaterialIndex(FbxNode* MeshNode, FbxMesh* Mesh, int32 PolygonIndex)
    {
        FbxSurfaceMaterial* FbxMaterial = ResolvePolygonFbxMaterial(MeshNode, Mesh, PolygonIndex);
        return FindOrAddFbxMaterial(FbxMaterial, SourcePath, Materials, BuildContext);
    }

    uint32 AddCornerVertex(
        FbxMesh*                    Mesh,
        int32                       PolygonIndex,
        int32                       CornerIndex,
        int32                       PolygonVertexIndex,
        const FFbxTriangleSample&   Triangle,
        const FFbxMeshImportSpace&  ImportSpace
        )
    {
        const int32 ControlPointIndex = Triangle.ControlPointIndices[CornerIndex];

        FNormalVertex Vertex;
        Vertex.pos     = Triangle.Positions[CornerIndex];
        Vertex.normal  = ReadCornerNormal(Mesh, PolygonIndex, CornerIndex, Triangle, ImportSpace);
        Vertex.tex     = Triangle.UV0[CornerIndex];
        Vertex.color   = ReadVertexColor(Mesh, ControlPointIndex, PolygonVertexIndex);
        Vertex.tangent = ReadCornerTangent(Mesh, ControlPointIndex, PolygonVertexIndex, Triangle, ImportSpace, Vertex.normal);

        bool bAddedNewVertex = false;
        return FindOrAddStaticVertex(
            Vertex,
            Mesh,
            ControlPointIndex,
            VertexToIndex,
            MeshAsset.Vertices,
            bAddedNewVertex
        );
    }

    FVector ReadCornerNormal(
        FbxMesh*                    Mesh,
        int32                       PolygonIndex,
        int32                       CornerIndex,
        const FFbxTriangleSample&   Triangle,
        const FFbxMeshImportSpace&  ImportSpace
        ) const
    {
        FVector LocalNormal;
        if (TryReadNormal(Mesh, PolygonIndex, CornerIndex, LocalNormal))
        {
            return TransformNormalByMatrix(LocalNormal, ImportSpace.NormalTransform);
        }

        return Triangle.FallbackNormal;
    }

    FVector4 ReadCornerTangent(
        FbxMesh*                    Mesh,
        int32                       ControlPointIndex,
        int32                       PolygonVertexIndex,
        const FFbxTriangleSample&   Triangle,
        const FFbxMeshImportSpace&  ImportSpace,
        const FVector&              ImportedNormal
        ) const
    {
        FVector4 ImportedTangent;
        if (TryReadTangent(Mesh, ControlPointIndex, PolygonVertexIndex, ImportedTangent))
        {
            const FVector Tangent = TransformTangentByMatrix(
                FVector(ImportedTangent.X, ImportedTangent.Y, ImportedTangent.Z),
                ImportSpace.VertexTransform,
                ImportedNormal
            );

            return FVector4(Tangent.X, Tangent.Y, Tangent.Z, ImportedTangent.W);
        }

        const FVector Tangent = OrthogonalizeTangentToNormal(Triangle.FallbackTangent, ImportedNormal);
        return FVector4(Tangent.X, Tangent.Y, Tangent.Z, 1.0f);
    }

    bool Finalize()
    {
        BuildFinalImportedSections(SectionBuilds, Materials, MeshAsset.Indices, MeshAsset.Sections);

        if (MeshAsset.Vertices.empty() || MeshAsset.Indices.empty() || MeshAsset.Sections.empty())
        {
            return false;
        }

        MeshAsset.CacheBounds();
        return true;
    }
};

static bool BuildStaticMeshFromNodes(
    const FString&           SourcePath,
    const TArray<FbxNode*>&  MeshNodes,
    TArray<FStaticMaterial>& OutMaterials,
    FStaticMesh&             OutMesh,
    FImportBuildContext&     BuildContext
    )
{
    FFbxStaticMeshBuilder Builder(SourcePath, OutMaterials, OutMesh, BuildContext);
    return Builder.Build(MeshNodes);
}

// FBX 파일에서 static mesh geometry, transform, vertex attributes, material section 정보를 import한다.
bool FFbxImporter::ImportStaticMesh(const FString& SourcePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    OutMesh = FStaticMesh();
    OutMaterials.clear();

    FFbxSceneHandle SceneHandle;
    if (!LoadFbxScene(SourcePath, SceneHandle))
    {
        return false;
    }

    NormalizeFbxScene(SceneHandle.Scene);
    TriangulateScene(SceneHandle.Manager, SceneHandle.Scene);

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);

    if (MeshNodes.empty())
    {
        return false;
    }

    OutMesh.PathFileName = SourcePath;

    FImportBuildContext BuildContext;
    BuildContext.Summary.SourcePath = SourcePath;

    if (!BuildStaticMeshFromNodes(SourcePath, MeshNodes, OutMaterials, OutMesh, BuildContext))
    {
        OutMesh = FStaticMesh();
        OutMaterials.clear();
        return false;
    }

    OutMesh.PathFileName = SourcePath;
    return true;
}

// FBX 파일에서 skeletal mesh, skeleton, LOD, animation, morph target을 import한다.
bool FFbxImporter::ImportSkeletalMesh(const FString& SourcePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    OutMesh = FSkeletalMesh();
    OutMaterials.clear();

    FFbxSceneHandle     SceneHandle;
    FImportBuildContext BuildContext;
    BuildContext.Summary.SourcePath = SourcePath;

    if (!LoadFbxScene(SourcePath, SceneHandle))
    {
        return false;
    }

    NormalizeFbxScene(SceneHandle.Scene);

    TriangulateScene(SceneHandle.Manager, SceneHandle.Scene);

    OutMesh.PathFileName = SourcePath;

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);
    BuildContext.Summary.SourceMeshCount = static_cast<int32>(MeshNodes.size());

    TArray<FbxNode*> SkinnedMeshNodes;

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (Mesh && MeshHasSkin(Mesh))
        {
            SkinnedMeshNodes.push_back(MeshNode);
        }
    }

    BuildContext.Summary.ImportedSkinnedMeshCount = static_cast<int32>(SkinnedMeshNodes.size());

    if (SkinnedMeshNodes.empty())
    {
        return false;
    }

    TMap<FbxNode*, int32> BoneNodeToIndex;
    if (!BuildSkeletonFromSkinClusters(SkinnedMeshNodes, MeshNodes, OutMesh.Skeleton, BoneNodeToIndex))
    {
        return false;
    }

    TArray<FSkeletalImportMeshNode> ImportMeshNodes;
    ClassifySkeletalImportMeshNodes(MeshNodes, BoneNodeToIndex, ImportMeshNodes);

    TMap<int32, TArray<FbxNode*>> SkinnedMeshNodesByLOD;
    for (FbxNode* MeshNode : SkinnedMeshNodes)
    {
        const int32 LODIndex = GetSkeletalMeshLODIndex(MeshNode);
        SkinnedMeshNodesByLOD[LODIndex].push_back(MeshNode);
    }

    TMap<int32, TArray<FSkeletalImportMeshNode>> MeshNodesByLOD;
    for (const FSkeletalImportMeshNode& ImportNode : ImportMeshNodes)
    {
        if (ImportNode.Kind == ESkeletalImportMeshKind::Loose || !ImportNode.MeshNode)
        {
            continue;
        }

        const int32 LODIndex = GetSkeletalMeshLODIndex(ImportNode.MeshNode);
        MeshNodesByLOD[LODIndex].push_back(ImportNode);
    }

    TArray<int32> SortedLODIndices;
    for (const auto& Pair : MeshNodesByLOD)
    {
        SortedLODIndices.push_back(Pair.first);
    }

    std::sort(SortedLODIndices.begin(), SortedLODIndices.end());

    if (SortedLODIndices.empty())
    {
        return false;
    }

    TArray<int32> SortedSkinnedLODIndices;
    for (const auto& Pair : SkinnedMeshNodesByLOD)
    {
        SortedSkinnedLODIndices.push_back(Pair.first);
    }

    std::sort(SortedSkinnedLODIndices.begin(), SortedSkinnedLODIndices.end());

    if (SortedSkinnedLODIndices.empty())
    {
        return false;
    }

    const int32             ReferenceLODIndex = SortedSkinnedLODIndices[0];
    const TArray<FbxNode*>& ReferenceLODNodes = SkinnedMeshNodesByLOD[ReferenceLODIndex];

    FMatrix ReferenceMeshBind;
    if (!TryGetReferenceMeshBindMatrix(SceneHandle.Scene, ReferenceLODNodes, ReferenceMeshBind))
    {
        return false;
    }

    const FMatrix ReferenceMeshBindInverse = ReferenceMeshBind.GetInverse();

    InitializeBoneBindPoseFromSceneNodes(BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton);

    TArray<bool> AppliedClusterBindPose;
    AppliedClusterBindPose.resize(OutMesh.Skeleton.Bones.size());

    ApplyBindPoseFromFbxPose(SceneHandle.Scene, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, AppliedClusterBindPose, BuildContext);

    ApplyBindPoseFromSkinClusters(ReferenceLODNodes, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, &AppliedClusterBindPose);
    ApplyBindPoseFromSkinClusters(SkinnedMeshNodes, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, &AppliedClusterBindPose);

    RecomputeLocalBindPose(OutMesh.Skeleton);

    TArray<TArray<FImportedMorphSourceVertex>> MorphSourcesByLOD;

    for (int32 LODIndex : SortedLODIndices)
    {
        FSkeletalMeshLOD                   NewLOD;
        TArray<FImportedMorphSourceVertex> MorphSources;

        if (!BuildSkeletalMeshLODFromNodes(
            SceneHandle.Scene,
            SourcePath,
            MeshNodesByLOD[LODIndex],
            BoneNodeToIndex,
            ReferenceMeshBindInverse, 
            OutMesh.Skeleton,
            OutMaterials,
            NewLOD,
            &MorphSources,
            BuildContext
        ))
        {
            continue;
        }

        OutMesh.LODModels.push_back(NewLOD);
        MorphSourcesByLOD.push_back(std::move(MorphSources));
    }

    if (OutMesh.LODModels.empty())
    {
        return false;
    }

    ImportMorphTargets(MorphSourcesByLOD, OutMesh.MorphTargets);

    ImportAnimations(SceneHandle.Scene, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, OutMesh.Animations);

    float MaxBindPoseError = 0.0f;
    for (const FSkeletalMeshLOD& LOD : OutMesh.LODModels)
    {
        MaxBindPoseError = (std::max)(MaxBindPoseError, ValidateBindPoseSkinningError(LOD, OutMesh.Skeleton));
    }

    BuildContext.Summary.MaxBindPoseValidationError = MaxBindPoseError;

    if (MaxBindPoseError > 0.001f)
    {
        BuildContext.AddWarning(ESkeletalImportWarningType::MissingBindPose, "Bind pose validation error is larger than tolerance.");
    }

    BuildContext.Summary.BoneCount          = static_cast<int32>(OutMesh.Skeleton.Bones.size());
    BuildContext.Summary.LODCount           = static_cast<int32>(OutMesh.LODModels.size());
    BuildContext.Summary.MaterialSlotCount  = static_cast<int32>(OutMaterials.size());
    BuildContext.Summary.AnimationClipCount = static_cast<int32>(OutMesh.Animations.size());
    BuildContext.Summary.MorphTargetCount   = static_cast<int32>(OutMesh.MorphTargets.size());

    if (BuildContext.Summary.CandidateVertexCount > 0)
    {
        BuildContext.Summary.DeduplicationRatio = static_cast<float>(BuildContext.Summary.DeduplicatedVertexCount) / static_cast<float>(BuildContext.Summary.
            CandidateVertexCount);
    }
    else
    {
        BuildContext.Summary.DeduplicationRatio = 0.0f;
    }

    OutMesh.ImportSummary = BuildContext.Summary;

    return true;
}

// FBX 파일에 skin deformer가 포함된 mesh가 있는지 검사한다.
bool FFbxImporter::HasSkinDeformer(const FString& SourcePath)
{
    FFbxSceneHandle SceneHandle;
    if (!LoadFbxScene(SourcePath, SceneHandle))
    {
        return false;
    }

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (MeshHasSkin(Mesh))
        {
            return true;
        }
    }

    return false;
}
