#include "PhysicsAssetManager.h"

#include "Asset/AssetPackage.h"
#include "PhysicsBodySetup.h"
#include "PhysicsShapeSetup.h"
#include "Object/FUObjectArray.h"
#include "PhysicsAsset.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <filesystem>
#include <utility>

namespace
{
    const char* ToString(EPhysicsBodyType Type)
    {
        switch (Type)
        {
        case EPhysicsBodyType::PBT_Static: return "Static";
        case EPhysicsBodyType::PBT_Dynamic: return "Dynamic";
        case EPhysicsBodyType::PBT_Kinematic: return "Kinematic";
        default: return "Unknown";
        }
    }

    const char* ToString(EPhysicsBodyCollisionComplexity Complexity)
    {
        switch (Complexity)
        {
        case EPhysicsBodyCollisionComplexity::ProjectDefault: return "ProjectDefault";
        case EPhysicsBodyCollisionComplexity::UseSimpleAsComplex: return "UseSimpleAsComplex";
        case EPhysicsBodyCollisionComplexity::UseComplexAsSimple: return "UseComplexAsSimple";
        default: return "Unknown";
        }
    }

    const char* ToString(EPhysicsCollisionEnabled Enabled)
    {
        switch (Enabled)
        {
        case EPhysicsCollisionEnabled::PCE_NoCollision: return "NoCollision";
        case EPhysicsCollisionEnabled::PCE_QueryOnly: return "QueryOnly";
        case EPhysicsCollisionEnabled::PCE_PhysicsOnly: return "PhysicsOnly";
        case EPhysicsCollisionEnabled::PCE_QueryAndPhysics: return "QueryAndPhysics";
        default: return "Unknown";
        }
    }

    const char* ToString(EPhysicsCollisionChannel Channel)
    {
        switch (Channel)
        {
        case EPhysicsCollisionChannel::PCC_WorldStatic: return "WorldStatic";
        case EPhysicsCollisionChannel::PCC_WorldDynamic: return "WorldDynamic";
        case EPhysicsCollisionChannel::PCC_Pawn: return "Pawn";
        case EPhysicsCollisionChannel::PCC_PhysicsBody: return "PhysicsBody";
        case EPhysicsCollisionChannel::PCC_Visibility: return "Visibility";
        case EPhysicsCollisionChannel::PCC_Camera: return "Camera";
        case EPhysicsCollisionChannel::PCC_Custom: return "Custom";
        default: return "Unknown";
        }
    }

    const char* ToString(EPhysicsCollisionResponse Response)
    {
        switch (Response)
        {
        case EPhysicsCollisionResponse::PCR_Ignore: return "Ignore";
        case EPhysicsCollisionResponse::PCR_Overlap: return "Overlap";
        case EPhysicsCollisionResponse::PCR_Block: return "Block";
        default: return "Unknown";
        }
    }

    const char* ToString(EPhysicsJointType JointType)
    {
        switch (JointType)
        {
        case EPhysicsJointType::PJT_Fixed: return "Fixed";
        case EPhysicsJointType::PJT_Distance: return "Distance";
        case EPhysicsJointType::PJT_Revolute: return "Revolute";
        case EPhysicsJointType::PJT_Prismatic: return "Prismatic";
        case EPhysicsJointType::PJT_Spherical: return "Spherical";
        case EPhysicsJointType::PJT_D6: return "D6";
        default: return "Unknown";
        }
    }

    const char* ToString(EPhysicsConstraintMotionMode Motion)
    {
        switch (Motion)
        {
        case EPhysicsConstraintMotionMode::Free: return "Free";
        case EPhysicsConstraintMotionMode::Limited: return "Limited";
        case EPhysicsConstraintMotionMode::Locked: return "Locked";
        default: return "Unknown";
        }
    }

    json::JSON SerializeVector(const FVector& V)
    {
        json::JSON Arr = json::Array();
        Arr.append(V.X);
        Arr.append(V.Y);
        Arr.append(V.Z);
        return Arr;
    }

    json::JSON SerializeQuat(const FQuat& Q)
    {
        json::JSON Arr = json::Array();
        Arr.append(Q.X);
        Arr.append(Q.Y);
        Arr.append(Q.Z);
        Arr.append(Q.W);
        return Arr;
    }

    json::JSON SerializeTransform(const FTransform& Transform)
    {
        json::JSON Obj = json::Object();
        Obj["Location"] = SerializeVector(Transform.Location);
        Obj["Rotation"] = SerializeQuat(Transform.Rotation);
        Obj["Scale"] = SerializeVector(Transform.Scale);
        return Obj;
    }

    json::JSON SerializeCollisionDesc(const FPhysicsCollisionDesc& Desc)
    {
        json::JSON Obj = json::Object();
        Obj["CollisionEnabled"] = ToString(Desc.CollisionEnabled);
        Obj["ObjectChannel"] = ToString(Desc.ObjectChannel);

        json::JSON Responses = json::Array();
        for (const FPhysicsCollisionResponsePair& Pair : Desc.Responses)
        {
            json::JSON ResponseObj = json::Object();
            ResponseObj["Channel"] = ToString(Pair.Channel);
            ResponseObj["Response"] = ToString(Pair.Response);
            Responses.append(ResponseObj);
        }
        Obj["Responses"] = Responses;
        return Obj;
    }

    void AddShapeCommon(json::JSON& Obj, const FName& Name, const FTransform& LocalTransform,
        UPhysicalMaterial* PhysicalMaterial, const FPhysicsCollisionDesc& CollisionDesc)
    {
        Obj["Name"] = Name.ToString();
        Obj["LocalTransform"] = SerializeTransform(LocalTransform);
        Obj["PhysicalMaterial"] = GetPhysicalMaterialAssetPath(PhysicalMaterial);
        Obj["Collision"] = SerializeCollisionDesc(CollisionDesc);
    }

    json::JSON SerializeShape(const FPhysicsSphereShapeSetup& Shape)
    {
        json::JSON Obj = json::Object();
        Obj["Type"] = "Sphere";
        AddShapeCommon(Obj, Shape.Name, Shape.LocalTransform, Shape.PhysicalMaterial, Shape.CollisionDesc);
        Obj["Radius"] = Shape.Radius;
        return Obj;
    }

    json::JSON SerializeShape(const FPhysicsBoxShapeSetup& Shape)
    {
        json::JSON Obj = json::Object();
        Obj["Type"] = "Box";
        AddShapeCommon(Obj, Shape.Name, Shape.LocalTransform, Shape.PhysicalMaterial, Shape.CollisionDesc);
        Obj["HalfExtent"] = SerializeVector(Shape.HalfExtent);
        return Obj;
    }

    json::JSON SerializeShape(const FPhysicsCapsuleShapeSetup& Shape)
    {
        json::JSON Obj = json::Object();
        Obj["Type"] = "Capsule";
        AddShapeCommon(Obj, Shape.Name, Shape.LocalTransform, Shape.PhysicalMaterial, Shape.CollisionDesc);
        Obj["Radius"] = Shape.Radius;
        Obj["Length"] = Shape.Length;
        return Obj;
    }

    json::JSON SerializeShape(const FPhysicsConvexShapeSetup& Shape)
    {
        json::JSON Obj = json::Object();
        Obj["Type"] = "Convex";
        AddShapeCommon(Obj, Shape.Name, Shape.LocalTransform, Shape.PhysicalMaterial, Shape.CollisionDesc);

        json::JSON Vertices = json::Array();
        for (const FVector& Vertex : Shape.VertexData)
        {
            Vertices.append(SerializeVector(Vertex));
        }
        Obj["VertexData"] = Vertices;
        return Obj;
    }

    json::JSON SerializeBodySetup(const UPhysicsBodySetup* BodySetup)
    {
        json::JSON Obj = json::Object();
        if (!BodySetup)
        {
            Obj["IsNull"] = true;
            return Obj;
        }

        Obj["TargetBoneName"] = BodySetup->GetTargetBoneName().ToString();
        Obj["BodyType"] = ToString(BodySetup->GetBodyType());
        Obj["Mass"] = BodySetup->GetMass();
        Obj["LinearDamping"] = BodySetup->GetLinearDamping();
        Obj["AngularDamping"] = BodySetup->GetAngularDamping();
        Obj["Collision"] = SerializeCollisionDesc(BodySetup->GetCollisionDesc());
        Obj["bOverrideMass"] = BodySetup->bOverrideMass;
        Obj["bEnableGravity"] = BodySetup->bEnableGravity;
        Obj["GravityGroupIndex"] = BodySetup->GravityGroupIndex;
        Obj["bUpdateKinematicFromSimulation"] = BodySetup->bUpdateKinematicFromSimulation;
        Obj["bGyroscopicTorqueEnabled"] = BodySetup->bGyroscopicTorqueEnabled;
        Obj["bDoubleSidedGeometry"] = BodySetup->bDoubleSidedGeometry;
        Obj["SimpleCollisionPhysicalMaterial"] = GetPhysicalMaterialAssetPath(BodySetup->SimpleCollisionPhysicalMaterial);
        Obj["PhysMaterialOverride"] = GetPhysicalMaterialAssetPath(BodySetup->PhysMaterialOverride);
        Obj["bSkipScaleFromAnimation"] = BodySetup->bSkipScaleFromAnimation;
        Obj["bConsiderForBounds"] = BodySetup->bConsiderForBounds;
        Obj["bSimulationGeneratesHitEvents"] = BodySetup->bSimulationGeneratesHitEvents;
        Obj["bNeverNeedsCookedCollisionData"] = BodySetup->bNeverNeedsCookedCollisionData;
        Obj["CollisionComplexity"] = ToString(BodySetup->CollisionComplexity);

        json::JSON Shapes = json::Array();
        const FPhysicsAggregateShapeSetup& ShapeSetup = BodySetup->GetShapeSetup();
        for (const FPhysicsSphereShapeSetup& Shape : ShapeSetup.SphereShapeSetups)
        {
            Shapes.append(SerializeShape(Shape));
        }
        for (const FPhysicsBoxShapeSetup& Shape : ShapeSetup.BoxShapeSetups)
        {
            Shapes.append(SerializeShape(Shape));
        }
        for (const FPhysicsCapsuleShapeSetup& Shape : ShapeSetup.CapsuleShapeSetups)
        {
            Shapes.append(SerializeShape(Shape));
        }
        for (const FPhysicsConvexShapeSetup& Shape : ShapeSetup.ConvexShapeSetups)
        {
            Shapes.append(SerializeShape(Shape));
        }
        Obj["Shapes"] = Shapes;
        return Obj;
    }

    json::JSON SerializeConstraintSetup(const FPhysicsConstraintSetup& ConstraintSetup)
    {
        json::JSON Obj = json::Object();
        Obj["ConstraintName"] = ConstraintSetup.ConstraintName.ToString();
        Obj["ParentBoneName"] = ConstraintSetup.ParentBoneName.ToString();
        Obj["ChildBoneName"] = ConstraintSetup.ChildBoneName.ToString();
        Obj["JointType"] = ToString(ConstraintSetup.JointType);
        Obj["ParentLocalFrame"] = SerializeTransform(ConstraintSetup.ParentLocalFrame);
        Obj["ChildLocalFrame"] = SerializeTransform(ConstraintSetup.ChildLocalFrame);
        Obj["LinearLimit"] = ConstraintSetup.LinearLimit;
        Obj["AngularLimit"] = ConstraintSetup.AngularLimit;
        Obj["TwistLimitMin"] = ConstraintSetup.TwistLimitMin;
        Obj["TwistLimitMax"] = ConstraintSetup.TwistLimitMax;
        Obj["SwingLimitY"] = ConstraintSetup.SwingLimitY;
        Obj["SwingLimitZ"] = ConstraintSetup.SwingLimitZ;
        Obj["bDisableCollision"] = ConstraintSetup.bDisableCollision;
        Obj["Stiffness"] = ConstraintSetup.Stiffness;
        Obj["Damping"] = ConstraintSetup.Damping;
        Obj["bParentDominates"] = ConstraintSetup.bParentDominates;
        Obj["bEnableMassConditioning"] = ConstraintSetup.bEnableMassConditioning;
        Obj["bUseLinearJointSolver"] = ConstraintSetup.bUseLinearJointSolver;
        Obj["bScaleLinearLimits"] = ConstraintSetup.bScaleLinearLimits;
        Obj["XMotion"] = ToString(ConstraintSetup.XMotion);
        Obj["YMotion"] = ToString(ConstraintSetup.YMotion);
        Obj["ZMotion"] = ToString(ConstraintSetup.ZMotion);
        Obj["Swing1Motion"] = ToString(ConstraintSetup.Swing1Motion);
        Obj["Swing2Motion"] = ToString(ConstraintSetup.Swing2Motion);
        Obj["TwistMotion"] = ToString(ConstraintSetup.TwistMotion);
        return Obj;
    }

    FString MakeDefaultJsonPath(const UPhysicsAsset* Asset)
    {
        std::filesystem::path Path(FPaths::ToWide(Asset->GetAssetPathFileName()));
        Path.replace_extension(L".json");
        return FPaths::ToUtf8(Path.generic_wstring());
    }
}

UPhysicsAsset* FPhysicsAssetManager::Load(const FString& Path)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

    auto It = LoadedAssets.find(NormalizedPath);
    if (It != LoadedAssets.end())
    {
        return It->second;
    }

    if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
    {
        return nullptr;
    }

    FWindowsBinReader Ar(NormalizedPath);
    if (!Ar.IsValid())
    {
        return nullptr;
    }

    FAssetPackageHeader Header;
    Ar << Header;
    if (!Header.IsValid(EAssetPackageType::PhysicsAsset))
    {
        return nullptr;
    }

    FAssetImportMetadata Metadata;
    Ar << Metadata;

    UPhysicsAsset* NewAsset = GUObjectArray.CreateObject<UPhysicsAsset>();
    NewAsset->Serialize(Ar);

    if (!Ar.IsValid())
    {
        GUObjectArray.DestroyObject(NewAsset);
        return nullptr;
    }

    NewAsset->SetAssetPathFileName(NormalizedPath);
    LoadedAssets.emplace(NormalizedPath, NewAsset);
    return NewAsset;
}

UPhysicsAsset* FPhysicsAssetManager::Find(const FString& Path) const
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
    auto It = LoadedAssets.find(NormalizedPath);
    return It != LoadedAssets.end() ? It->second : nullptr;
}

bool FPhysicsAssetManager::Unload(const FString& Path)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
    auto It = LoadedAssets.find(NormalizedPath);
    if (It == LoadedAssets.end())
    {
        return false;
    }

    if (It->second)
    {
        GUObjectArray.DestroyObject(It->second);
    }

    LoadedAssets.erase(It);
    return true;
}

bool FPhysicsAssetManager::Save(UPhysicsAsset* Asset)
{
    if (!Asset)
    {
        return false;
    }

    const FString& Path = Asset->GetAssetPathFileName();
    if (Path.empty() || !FAssetPackage::IsAssetPackagePath(Path))
    {
        return false;
    }

    FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
    if (!Ar.IsValid())
    {
        return false;
    }

    FAssetPackageHeader Header;
    Header.Type = static_cast<uint32>(EAssetPackageType::PhysicsAsset);

    FAssetImportMetadata Metadata;

    Ar << Header;
    Ar << Metadata;
    Asset->Serialize(Ar);

    return Ar.IsValid();
}

bool FPhysicsAssetManager::SaveAsJson(UPhysicsAsset* Asset, const FString& OverridePath)
{
    if (!Asset)
    {
        return false;
    }

    const FString JsonPath = OverridePath.empty() ? MakeDefaultJsonPath(Asset) : OverridePath;
    if (JsonPath.empty())
    {
        return false;
    }

    json::JSON Root = json::Object();
    Root["ClassName"] = "PhysicsAsset";
    Root["AssetPath"] = Asset->GetAssetPathFileName();
    Root["PreviewSkeletalMeshPath"] = Asset->GetPreviewSkeletalMeshPath();
    Root["RagdollMode"] = PhysicsAssetRagdollModeToString(Asset->GetRagdollMode());

    json::JSON GraphView = json::Object();
    GraphView["Pan"] = SerializeVector(FVector(Asset->GetGraphViewState().Pan.X, Asset->GetGraphViewState().Pan.Y, 0.0f));
    GraphView["Zoom"] = Asset->GetGraphViewState().Zoom;
    json::JSON NodeLayouts = json::Array();
    for (const FPhysicsAssetGraphNodeLayout& NodeLayout : Asset->GetGraphViewState().NodeLayouts)
    {
        json::JSON NodeObj = json::Object();
        NodeObj["NodeKey"] = NodeLayout.NodeKey;
        NodeObj["Position"] = SerializeVector(FVector(NodeLayout.Position.X, NodeLayout.Position.Y, 0.0f));
        NodeLayouts.append(NodeObj);
    }
    GraphView["NodeLayouts"] = NodeLayouts;
    Root["GraphViewState"] = GraphView;

    json::JSON Bodies = json::Array();
    for (const UPhysicsBodySetup* BodySetup : Asset->GetBodySetups())
    {
        Bodies.append(SerializeBodySetup(BodySetup));
    }
    Root["Bodies"] = Bodies;

    json::JSON Constraints = json::Array();
    for (const FPhysicsConstraintSetup& ConstraintSetup : Asset->GetConstraintSetups())
    {
        Constraints.append(SerializeConstraintSetup(ConstraintSetup));
    }
    Root["Constraints"] = Constraints;

    std::filesystem::path OutputPath(FPaths::ToWide(JsonPath));
    if (!OutputPath.is_absolute())
    {
        OutputPath = std::filesystem::path(FPaths::RootDir()) / OutputPath;
    }

    std::error_code ErrorCode;
    if (OutputPath.has_parent_path())
    {
        std::filesystem::create_directories(OutputPath.parent_path(), ErrorCode);
        if (ErrorCode)
        {
            return false;
        }
    }

    std::ofstream File(OutputPath, std::ios::binary | std::ios::trunc);
    if (!File.is_open())
    {
        return false;
    }

    File << Root.dump(4);
    File.flush();
    return File.good();
}

void FPhysicsAssetManager::ScanPhysicsAssets()
{
    AvailablePhysicsAssetFiles.clear();

    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    const std::filesystem::path AssetRoots[] = {
        ProjectRoot / L"Asset",
        ProjectRoot / L"Content",
        ProjectRoot / L"Data",
    };

    for (const std::filesystem::path& AssetRoot : AssetRoots)
    {
        if (!std::filesystem::exists(AssetRoot))
        {
            continue;
        }

        for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
        {
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path& Path = Entry.path();
            std::wstring Extension = Path.extension().wstring();
            std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
            if (Extension != L".uasset")
            {
                continue;
            }

            const FString RelPath = FPaths::MakeProjectRelative(
                FPaths::ToUtf8(Path.lexically_normal().generic_wstring()));

            EAssetPackageType PackageType = EAssetPackageType::Unknown;
            if (!FAssetPackage::GetPackageType(RelPath, PackageType)
                || PackageType != EAssetPackageType::PhysicsAsset)
            {
                continue;
            }

            FPhysicsAssetListItem Item;
            Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
            Item.FullPath = RelPath;
            AvailablePhysicsAssetFiles.push_back(std::move(Item));
        }
    }

    std::sort(AvailablePhysicsAssetFiles.begin(), AvailablePhysicsAssetFiles.end(),
        [](const FPhysicsAssetListItem& A, const FPhysicsAssetListItem& B)
        {
            return A.FullPath < B.FullPath;
        });
}
