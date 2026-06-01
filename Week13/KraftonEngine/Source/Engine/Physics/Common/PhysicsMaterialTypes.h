#pragma once

#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

/**
 * @file PhysicsMaterialTypes.h
 * @brief 물리 재질 설정과 물리 재질 Asset 정의.
 */

/** 물리 재질 결합 방식 */
enum class EPhysicsMaterialCombineMode : uint8
{
	Average = 0,
	Min,
	Multiply,
	Max,
};

/** 소프트 충돌 처리 모드 */
enum class EPhysicsMaterialSoftCollisionMode : uint8
{
	None = 0,
	Simple,
	Detailed,
};

/** 물리 재질 Asset */
class UPhysicalMaterial : public UObject
{
public:
	DECLARE_CLASS(UPhysicalMaterial, UObject)

	UPhysicalMaterial() = default;
	~UPhysicalMaterial() override = default;

	const FString& GetAssetPathFileName() const override { return AssetPathFileName; }
	void SetAssetPathFileName(const FString& InPath) { AssetPathFileName = InPath; }

	void Serialize(FArchive& Ar) override;

public:
	/** 물리 재질 설정 Desc */
	float Friction = 0.7f;
	float StaticFriction = 0.0f;
	EPhysicsMaterialCombineMode FrictionCombineMode = EPhysicsMaterialCombineMode::Average;
	bool bOverrideFrictionCombineMode = false;

	float Restitution = 0.3f;
	EPhysicsMaterialCombineMode RestitutionCombineMode = EPhysicsMaterialCombineMode::Average;
	bool bOverrideRestitutionCombineMode = false;

	float Density = 1.0f;
	float SleepLinearVelocityThreshold = 1.0f;
	float SleepAngularVelocityThreshold = 0.05f;
	int32 SleepCounterThreshold = 4;

	float RaiseMassToPower = 0.75f;
	FString SurfaceType = "Default";
	float Strength = 0.0f;
	float DamageModifier = 1.0f;

	bool bShowExperimentalProperties = false;
	EPhysicsMaterialSoftCollisionMode SoftCollisionMode = EPhysicsMaterialSoftCollisionMode::None;
	float SoftCollisionThickness = 0.0f;
	float BaseFrictionImpulse = 0.0f;

private:
	FString AssetPathFileName;
};
