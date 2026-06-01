#pragma once

#include "Core/CoreTypes.h"

#include <d3d11.h>

enum class EMaterialCreatePreset
{
	UberLit,
	Particle,
};

enum class EPhysicsAssetPrimitiveType : uint8
{
	Capsule,
	Box,
	Sphere,
};

enum class EPhysicsAssetVertexWeightingType : uint8
{
	DominantWeight,
	AnyWeight,
};

enum class EPhysicsAssetBodyGenerationMethod : uint8
{
	VertexWeight,
	BoneLength,
};

enum class EPhysicsAssetAngularConstraintMode : uint8
{
	Limited,
	Locked,
	Free,
};

struct FPhysicsAssetCreateParams
{
	float MinBoneSize = 20.0f;
	EPhysicsAssetPrimitiveType PrimitiveType = EPhysicsAssetPrimitiveType::Capsule;
	EPhysicsAssetBodyGenerationMethod BodyGenerationMethod = EPhysicsAssetBodyGenerationMethod::VertexWeight;
	EPhysicsAssetVertexWeightingType VertexWeightingType = EPhysicsAssetVertexWeightingType::DominantWeight;
	float MinVertexWeight = 0.0f; // Reference-style default: dominant weighting ignores this; AnyWeight uses > 0 by default.
	int32 MinVertexCountForFit = 1; // Kept for UI/backward compatibility; reference-style generation uses MinBoneSize instead.
	float MinWeldSize = 1.0e-4f;
	float MinPrimitiveSize = 0.1f;
	float FitPadding = 1.01f;
	bool bAutoOrientToBone = true;
	bool bWalkPastSmallBones = true;
	bool bCreateBodyForAllBones = false;
	bool bFallbackToBoneLength = true; // Used only when vertex/weight data cannot produce any body.
	bool bDisableCollisionsByDefault = true;
	int32 LodIndex = 0;
	bool bCreateConstraints = true;
	EPhysicsAssetAngularConstraintMode AngularConstraintMode = EPhysicsAssetAngularConstraintMode::Limited;
};

class UPhysicsAsset;

class FAssetFactory
{
public:
	static bool CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateAnimInstanceAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateParticleSystemAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreatePhysicsAsset(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreatePhysicsAssetFromSkeletalMesh(
		const FString& DirectoryPath,
		const FString& AssetName,
		const FString& SkeletalMeshPath,
		const FPhysicsAssetCreateParams& CreateParams,
		ID3D11Device* Device,
		FString& OutCreatedPath);
	static bool PopulatePhysicsAssetFromSkeletalMesh(
		UPhysicsAsset* PhysicsAsset,
		const FString& SkeletalMeshPath,
		const FPhysicsAssetCreateParams& CreateParams,
		ID3D11Device* Device);
	static const FString& GetLastPhysicsAssetCreateWarning();
	static bool CreateMaterial(const FString& DirectoryPath, const FString& AssetName, EMaterialCreatePreset Preset, FString& OutCreatedPath);
};
