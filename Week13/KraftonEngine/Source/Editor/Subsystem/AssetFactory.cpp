#include "Editor/Subsystem/AssetFactory.h"

#include "Editor/Subsystem/PhysicsAssetGenerator.h"

#include "Animation/AnimInstanceAsset.h"
#include "Animation/AnimInstanceAssetManager.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "FloatCurve/FloatCurveManager.h"
#include "Object/FUObjectArray.h"
#include "Object/ObjectFactory.h"
#include "Particles/Assets/ParticleAsset.h"
#include "Particles/Assets/ParticleSystemAssetManager.h"
#include "Physics/Assets/PhysicsAsset.h"
#include "Physics/Assets/PhysicsAssetManager.h"
#include "Physics/Common/PhysicalMaterialManager.h"
#include "Physics/Common/PhysicsMaterialTypes.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <filesystem>
#include <fstream>

namespace
{
	FString SanitizeAssetStem(const FString& AssetName)
	{
		return AssetName.empty() ? FString("NewFloatCurve") : AssetName;
	}

	std::filesystem::path BuildUniqueAssetPath(const std::filesystem::path& Directory, const FString& AssetName, const wchar_t* Extension)
	{
		const FString BaseStem = SanitizeAssetStem(AssetName);

		int32 Suffix = 0;
		for (;;)
		{
			FString CandidateStem = BaseStem;
			if (Suffix > 0)
			{
				CandidateStem += "_";
				CandidateStem += std::to_string(Suffix);
			}

			std::filesystem::path CandidatePath = Directory / (FPaths::ToWide(CandidateStem) + Extension);
			if (!std::filesystem::exists(CandidatePath))
			{
				return CandidatePath;
			}

			++Suffix;
		}
	}

	void InitializeDefaultParticleSystem(UParticleSystem* NewAsset)
	{
		if (!NewAsset)
		{
			return;
		}

		UParticleEmitter* Emitter = GUObjectArray.CreateObject<UParticleEmitter>(NewAsset);
		UParticleLODLevel* LOD = GUObjectArray.CreateObject<UParticleLODLevel>(Emitter);

		UParticleModuleRequired* Required = GUObjectArray.CreateObject<UParticleModuleRequired>(LOD);
		UParticleModuleTypeDataSprite* TypeData = GUObjectArray.CreateObject<UParticleModuleTypeDataSprite>(LOD);
		UParticleModuleSpawn* Spawn = GUObjectArray.CreateObject<UParticleModuleSpawn>(LOD);
		UParticleModuleLifetime* Lifetime = GUObjectArray.CreateObject<UParticleModuleLifetime>(LOD);
		UParticleModuleVelocity* Velocity = GUObjectArray.CreateObject<UParticleModuleVelocity>(LOD);

		LOD->SetRequiredModule(Required);
		LOD->SetTypeDataModule(TypeData);
		LOD->AddModule(Spawn);
		LOD->AddModule(Lifetime);
		LOD->AddModule(Velocity);
		LOD->CacheModules();

		Emitter->AddLODLevel(LOD);
		NewAsset->AddEmitter(Emitter);
		NewAsset->CacheSystemModuleInfo();
	}

	void InitializeMaterialJson(json::JSON& JsonData, const FString& MaterialPath, EMaterialCreatePreset Preset)
	{
		JsonData["PathFileName"] = MaterialPath;
		JsonData["Origin"] = "Editor";

		switch (Preset)
		{
		case EMaterialCreatePreset::Particle:
			JsonData["ShaderPath"] = "Shaders/Particles/ParticleSprite.hlsl";
			JsonData["BlendMode"] = "Translucent";
			JsonData["RenderPass"] = "AlphaBlend";
			JsonData["BlendState"] = "AlphaBlend";
			JsonData["DepthStencilState"] = "Default";
			JsonData["RasterizerState"] = "SolidNoCull";
			JsonData["Parameters"] = json::Object();
			JsonData["Textures"] = json::Object();
			break;

		case EMaterialCreatePreset::UberLit:
		default:
			JsonData["ShaderPath"] = "Shaders/Geometry/UberLit.hlsl";
			JsonData["BlendMode"] = "Opaque";
			JsonData["RenderPass"] = "Opaque";
			JsonData["BlendState"] = "Opaque";
			JsonData["DepthStencilState"] = "Default";
			JsonData["RasterizerState"] = "SolidBackCull";
			JsonData["Parameters"]["SectionColor"] = json::Array(1.0f, 1.0f, 1.0f, 1.0f);
			JsonData["Parameters"]["HasNormalMap"] = 0.0f;
			JsonData["Textures"] = json::Object();
			break;
		}
	}

	FString GetPhysicsAssetModeSuffix(EPhysicsAssetRagdollMode RagdollMode)
	{
		switch (RagdollMode)
		{
		case EPhysicsAssetRagdollMode::PxAggregate:
			return "_aggregate";
		case EPhysicsAssetRagdollMode::PerBody:
		default:
			return "_per_bone";
		}
	}

	FString MakeDefaultPhysicsAssetName(const FString& SkeletalMeshPath, EPhysicsAssetRagdollMode RagdollMode)
	{
		const std::filesystem::path MeshPath(FPaths::ToWide(SkeletalMeshPath));
		FString BaseName = FPaths::ToUtf8(MeshPath.stem().wstring());
		if (BaseName.empty())
		{
			BaseName = "NewPhysicsAsset";
		}

		BaseName += "_PhysicsAsset";
		BaseName += GetPhysicsAssetModeSuffix(RagdollMode);
		return BaseName;
	}
}

bool FAssetFactory::CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UFloatCurveAsset* NewAsset = GUObjectArray.CreateObject<UFloatCurveAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	FFloatCurve& Curve = NewAsset->GetCurve();
	Curve.Reset();
	Curve.AddKey(0.0f, 0.0f);
	Curve.AddKey(1.0f, 1.0f);
	Curve.SortKeys();

	bool bSaved = FFloatCurveManager::Get().Save(NewAsset);
	GUObjectArray.DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UCameraShakeAsset* NewAsset = GUObjectArray.CreateObject<UCameraShakeAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->Version = 1;
	NewAsset->ShakeType = ECameraShakeType::Sequence;

	bool bSaved = FCameraShakeManager::Get().Save(NewAsset);
	GUObjectArray.DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateAnimInstanceAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const FString DefaultName = AssetName.empty() ? FString("NewAnimInstance") : AssetName;
	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, DefaultName, L".uasset");

	UAnimInstanceAsset* NewAsset = GUObjectArray.CreateObject<UAnimInstanceAsset>();
	NewAsset->SetAssetPathFileName(FPaths::ToUtf8(AssetPath.wstring()));

	FAnimGraphData& Graph = NewAsset->GetGraph();

	FAnimGraphNodeData OutputNode;
	OutputNode.NodeId = "Output";
	OutputNode.DisplayName = "Output";
	OutputNode.NodeType = EAnimGraphNodeType::Output;
	OutputNode.EditorPosX = 360.0f;
	OutputNode.EditorPosY = 80.0f;

	Graph.OutputNodeId = OutputNode.NodeId;
	Graph.Nodes.push_back(OutputNode);

	bool bSaved = FAnimInstanceAssetManager::Get().Save(NewAsset);
	GUObjectArray.DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateParticleSystemAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UParticleSystem* NewAsset = GUObjectArray.CreateObject<UParticleSystem>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	InitializeDefaultParticleSystem(NewAsset);

	bool bSaved = FParticleSystemAssetManager::Get().Save(NewAsset);
	GUObjectArray.DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreatePhysicsAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const FString DefaultName = AssetName.empty() ? FString("NewPhysicsAsset") : AssetName;
	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, DefaultName, L".uasset");

	UPhysicsAsset* NewAsset = GUObjectArray.CreateObject<UPhysicsAsset>();
	NewAsset->SetAssetPathFileName(FPaths::ToUtf8(AssetPath.wstring()));

	const bool bSaved = FPhysicsAssetManager::Get().Save(NewAsset);
	GUObjectArray.DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreatePhysicalMaterial(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const FString DefaultName = AssetName.empty() ? FString("NewPhysicalMaterial") : AssetName;
	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, DefaultName, L".uasset");

	UPhysicalMaterial* NewAsset = GUObjectArray.CreateObject<UPhysicalMaterial>();
	NewAsset->SetAssetPathFileName(FPaths::ToUtf8(AssetPath.wstring()));

	const bool bSaved = FPhysicalMaterialManager::Get().Save(NewAsset);
	GUObjectArray.DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreatePhysicsAssetFromSkeletalMesh(
	const FString& DirectoryPath,
	const FString& AssetName,
	const FString& SkeletalMeshPath,
	const FPhysicsAssetCreateParams& CreateParams,
	ID3D11Device* Device,
	FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const FString DefaultName = AssetName.empty()
		? MakeDefaultPhysicsAssetName(SkeletalMeshPath, CreateParams.RagdollMode)
		: AssetName;
	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, DefaultName, L".uasset");

	UPhysicsAsset* NewAsset = GUObjectArray.CreateObject<UPhysicsAsset>();
	NewAsset->SetAssetPathFileName(FPaths::ToUtf8(AssetPath.wstring()));

	const bool bInitialized = PopulatePhysicsAssetFromSkeletalMesh(NewAsset, SkeletalMeshPath, CreateParams, Device);
	const bool bSaved = bInitialized && FPhysicsAssetManager::Get().Save(NewAsset);
	GUObjectArray.DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::PopulatePhysicsAssetFromSkeletalMesh(
	UPhysicsAsset* PhysicsAsset,
	const FString& SkeletalMeshPath,
	const FPhysicsAssetCreateParams& CreateParams,
	ID3D11Device* Device)
{
	return FPhysicsAssetGenerator::PopulatePhysicsAssetFromSkeletalMesh(PhysicsAsset, SkeletalMeshPath, CreateParams, Device);
}

const FString& FAssetFactory::GetLastPhysicsAssetCreateWarning()
{
	return FPhysicsAssetGenerator::GetLastPhysicsAssetCreateWarning();
}

bool FAssetFactory::CreateMaterial(const FString& DirectoryPath, const FString& AssetName, EMaterialCreatePreset Preset, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const FString DefaultName = AssetName.empty() ? FString("NewMaterial") : AssetName;
	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, DefaultName, L".mat");
	const FString AssetPathString = FPaths::ToUtf8(AssetPath.generic_wstring());
	const FString MaterialPath = FPaths::MakeProjectRelative(AssetPathString);

	json::JSON JsonData;
	InitializeMaterialJson(JsonData, MaterialPath, Preset);

	std::ofstream File(AssetPath);
	if (!File.is_open())
	{
		return false;
	}

	File << JsonData.dump();
	if (!File.good())
	{
		return false;
	}

	OutCreatedPath = MaterialPath;
	return true;
}
