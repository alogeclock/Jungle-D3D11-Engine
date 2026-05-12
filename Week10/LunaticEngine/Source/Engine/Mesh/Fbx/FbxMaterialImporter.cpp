#include "Mesh/Fbx/FbxMaterialImporter.h"

#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "SimpleJSON/json.hpp"

#include <fbxsdk.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

namespace
{
    // 파일명으로 쓰기 어려운 문자를 안전한 '_' 문자로 바꾼다.
    FString MakeSafeAssetName(const FString& Name)
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

    // float 값을 hash에 안정적으로 쓸 수 있는 32비트 표현으로 변환한다.
    uint32 FloatToStableBits(float Value)
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

    // size_t hash seed에 새 값을 섞는다.
    void HashCombineSizeT(std::size_t& Seed, std::size_t Value)
    {
        Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
    }

    // uint32 값을 material hash seed에 섞는다.
    void HashCombineUInt32(std::size_t& Seed, uint32 Value)
    {
        HashCombineSizeT(Seed, static_cast<std::size_t>(Value));
    }

    // 문자열 값을 material hash seed에 섞는다.
    void HashCombineString(std::size_t& Seed, const FString& Value)
    {
        for (char C : Value)
        {
            HashCombineSizeT(Seed, static_cast<std::size_t>(static_cast<unsigned char>(C)));
        }
    }

    // material 정보 전체를 자동 생성 파일명에 사용할 hash로 만든다.
    std::size_t MakeFbxMaterialInfoHash(const FFbxImportedMaterialInfo& Info)
    {
        std::size_t Seed = 0;

        HashCombineString(Seed, Info.SlotName);
        HashCombineString(Seed, Info.DiffuseTexture);
        HashCombineString(Seed, Info.NormalTexture);
        HashCombineString(Seed, Info.RoughnessTexture);
        HashCombineString(Seed, Info.MetallicTexture);
        HashCombineString(Seed, Info.EmissiveTexture);
        HashCombineString(Seed, Info.OpacityTexture);

        HashCombineUInt32(Seed, FloatToStableBits(Info.BaseColor.X));
        HashCombineUInt32(Seed, FloatToStableBits(Info.BaseColor.Y));
        HashCombineUInt32(Seed, FloatToStableBits(Info.BaseColor.Z));
        HashCombineUInt32(Seed, FloatToStableBits(Info.BaseColor.W));
        HashCombineUInt32(Seed, FloatToStableBits(Info.Roughness));
        HashCombineUInt32(Seed, FloatToStableBits(Info.Metallic));
        HashCombineUInt32(Seed, FloatToStableBits(Info.Opacity));
        HashCombineUInt32(Seed, Info.bHasOpacity ? 1u : 0u);

        return Seed;
    }

    // FBX file texture 경로를 엔진 asset 경로 기준으로 해석한다.
    bool TryResolveFbxFileTexturePath(FbxFileTexture* FileTexture, const FString& SourceFbxPath, FString& OutTexturePath)
    {
        if (!FileTexture)
        {
            return false;
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
            return false;
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

    // FBX texture property에서 첫 번째 유효 file texture 경로를 추출한다.
    bool TryGetFbxTexturePathFromProperty(const FbxProperty& Property, const FString& SourceFbxPath, FString& OutTexturePath, FFbxImportContext* Context = nullptr)
    {
        if (!Property.IsValid())
        {
            return false;
        }

        const int32 TextureCount = Property.GetSrcObjectCount<FbxFileTexture>();

        for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
        {
            FbxFileTexture* FileTexture = Property.GetSrcObject<FbxFileTexture>(TextureIndex);

            if (TryResolveFbxFileTexturePath(FileTexture, SourceFbxPath, OutTexturePath))
            {
                return true;
            }
        }

        const int32 LayeredTextureCount = Property.GetSrcObjectCount<FbxLayeredTexture>();
        if (LayeredTextureCount > 0 && Context)
        {
            Context->AddWarningOnce(
                ESkeletalImportWarningType::UnsupportedMaterialProperty,
                "Layered texture is partially imported. Only the first resolvable file texture is used."
            );
        }

        for (int32 LayeredTextureIndex = 0; LayeredTextureIndex < LayeredTextureCount; ++LayeredTextureIndex)
        {
            FbxLayeredTexture* LayeredTexture = Property.GetSrcObject<FbxLayeredTexture>(LayeredTextureIndex);
            if (!LayeredTexture)
            {
                continue;
            }

            const int32 LayerTextureCount = LayeredTexture->GetSrcObjectCount<FbxFileTexture>();

            for (int32 LayerTextureIndex = 0; LayerTextureIndex < LayerTextureCount; ++LayerTextureIndex)
            {
                FbxFileTexture* FileTexture = LayeredTexture->GetSrcObject<FbxFileTexture>(LayerTextureIndex);

                if (TryResolveFbxFileTexturePath(FileTexture, SourceFbxPath, OutTexturePath))
                {
                    return true;
                }
            }
        }

        return false;
    }

    // FBX material의 color property를 FVector4 base color로 읽는다.
    bool TryGetFbxColorProperty(FbxSurfaceMaterial* Material, const char* PropertyName, FVector4& OutColor)
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


    bool TryGetFbxDoubleProperty(FbxSurfaceMaterial* Material, const char* PropertyName, float& OutValue)
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

        OutValue = static_cast<float>(Property.Get<FbxDouble>());
        return true;
    }

    // FBX material에서 엔진 material 생성에 필요한 texture path와 base color를 추출한다.
    FFbxImportedMaterialInfo ExtractFbxMaterialInfo(FbxSurfaceMaterial* FbxMaterial, const FString& SourceFbxPath, FFbxImportContext& Context)
    {
        FFbxImportedMaterialInfo Info;
        Info.SlotName = FbxMaterial ? FbxMaterial->GetName() : "None";

        if (!FbxMaterial)
        {
            return Info;
        }

        TryGetFbxColorProperty(FbxMaterial, FbxSurfaceMaterial::sDiffuse, Info.BaseColor);

        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse), SourceFbxPath, Info.DiffuseTexture, &Context);
        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap), SourceFbxPath, Info.NormalTexture, &Context);

        if (Info.NormalTexture.empty())
        {
            TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sBump), SourceFbxPath, Info.NormalTexture, &Context);
        }

        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sEmissive), SourceFbxPath, Info.EmissiveTexture, &Context);
        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty("Roughness"), SourceFbxPath, Info.RoughnessTexture, &Context);

        if (!TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty("Metalness"), SourceFbxPath, Info.MetallicTexture, &Context))
        {
            TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty("Metallic"), SourceFbxPath, Info.MetallicTexture, &Context);
        }

        TryGetFbxTexturePathFromProperty(FbxMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor), SourceFbxPath, Info.OpacityTexture, &Context);

        TryGetFbxDoubleProperty(FbxMaterial, "Roughness", Info.Roughness);
        if (!TryGetFbxDoubleProperty(FbxMaterial, "Metalness", Info.Metallic))
        {
            TryGetFbxDoubleProperty(FbxMaterial, "Metallic", Info.Metallic);
        }

        float TransparencyFactor = 0.0f;
        if (TryGetFbxDoubleProperty(FbxMaterial, FbxSurfaceMaterial::sTransparencyFactor, TransparencyFactor))
        {
            Info.Opacity = 1.0f - TransparencyFactor;
        }

        if (!Info.OpacityTexture.empty() || Info.Opacity < 0.999f || Info.BaseColor.W < 0.999f)
        {
            Info.bHasOpacity = true;
        }

        return Info;
    }

    // FBX source와 slot 이름을 이용해 충돌 가능성이 낮은 자동 material asset path를 만든다.
    FString MakeAutoFbxMaterialPath(const FString& SourceFbxPath, const FFbxImportedMaterialInfo& Info)
    {
        const std::filesystem::path SourcePath      = FPaths::ToWide(SourceFbxPath);
        FString                     SourceAssetName = FPaths::ToUtf8(SourcePath.stem().wstring());

        if (SourceAssetName.empty())
        {
            SourceAssetName = "UnknownFbx";
        }

        const FString SafeSlotName = MakeSafeAssetName(Info.SlotName);
        const FString HashSuffix   = std::to_string(MakeFbxMaterialInfoHash(Info));

        return "Asset/Materials/Auto/Fbx/" + MakeSafeAssetName(SourceAssetName) + "/" + SafeSlotName + "-" + HashSuffix + ".mat";
    }

    // 추출한 FBX material 정보를 엔진 .mat JSON 파일로 저장하고 material asset path를 반환한다.
    FString ConvertFbxMaterialInfoToMat(const FFbxImportedMaterialInfo& Info, const FString& SourceFbxPath, FFbxImportContext& Context)
    {
        const FString               MatPath      = MakeAutoFbxMaterialPath(SourceFbxPath, Info);
        const FString               FullDiskPath = FPaths::ConvertRelativePathToFull(MatPath);
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
        JsonData["RenderPass"]   = Info.bHasOpacity ? "AlphaBlend" : "Opaque";
        if (Info.bHasOpacity)
        {
            JsonData["BlendState"] = "AlphaBlend";
            JsonData["DepthStencilState"] = "DepthReadOnly";
        }

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
            JsonData["Textures"]["OpacityTexture"] = Info.OpacityTexture;
        }

        JsonData["Parameters"]["SectionColor"][0] = Info.BaseColor.X;
        JsonData["Parameters"]["SectionColor"][1] = Info.BaseColor.Y;
        JsonData["Parameters"]["SectionColor"][2] = Info.BaseColor.Z;
        JsonData["Parameters"]["SectionColor"][3] = Info.BaseColor.W;
        JsonData["Parameters"]["Roughness"] = Info.Roughness;
        JsonData["Parameters"]["Metallic"] = Info.Metallic;
        JsonData["Parameters"]["Opacity"] = Info.Opacity;

        std::ofstream File(DiskPath, std::ios::binary);
        if (!File.is_open())
        {
            Context.AddWarning(ESkeletalImportWarningType::UnsupportedMaterialProperty, "Failed to create auto material file: " + MatPath);
            return "None";
        }

        File << JsonData.dump();
        return MatPath;
    }
}

// FBX material을 외부 texture path 기반 .mat 파일로 변환하고 material slot에 연결한다.
int32 FFbxMaterialImporter::FindOrAddMaterial(
    FbxSurfaceMaterial*      FbxMaterial,
    const FString&           SourceFbxPath,
    TArray<FStaticMaterial>& OutMaterials,
    FFbxImportContext&       Context
    )
{
    if (!FbxMaterial)
    {
        for (int32 i = 0; i < static_cast<int32>(OutMaterials.size()); ++i)
        {
            if (OutMaterials[i].MaterialSlotName == "None")
            {
                return i;
            }
        }

        FStaticMaterial NewMaterial;
        NewMaterial.MaterialSlotName  = "None";
        NewMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");

        OutMaterials.push_back(NewMaterial);
        return static_cast<int32>(OutMaterials.size()) - 1;
    }

    auto Existing = Context.FbxMaterialToIndex.find(FbxMaterial);
    if (Existing != Context.FbxMaterialToIndex.end())
    {
        return Existing->second;
    }

    const FString                  SlotName     = FbxMaterial ? FbxMaterial->GetName() : "None";
    const FFbxImportedMaterialInfo MaterialInfo = ExtractFbxMaterialInfo(FbxMaterial, SourceFbxPath, Context);
    const FString                  MaterialPath = ConvertFbxMaterialInfoToMat(MaterialInfo, SourceFbxPath, Context);

    FStaticMaterial NewMaterial;
    NewMaterial.MaterialSlotName  = SlotName;
    NewMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
    OutMaterials.push_back(NewMaterial);

    const int32 NewIndex                    = static_cast<int32>(OutMaterials.size()) - 1;
    Context.FbxMaterialToIndex[FbxMaterial] = NewIndex;
    return NewIndex;
}

// material이 없을 때 사용할 기본 None material slot을 보장한다.
void FFbxMaterialImporter::GetOrCreateDefaultMaterial(TArray<FStaticMaterial>& OutMaterials)
{
    for (const FStaticMaterial& Material : OutMaterials)
    {
        if (Material.MaterialSlotName == "None")
        {
            return;
        }
    }

    FStaticMaterial NewMaterial;
    NewMaterial.MaterialSlotName  = "None";
    NewMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");
    OutMaterials.push_back(NewMaterial);
}
