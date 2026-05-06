#pragma once
#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Rotator.h"
#include <Object/FName.h>
struct FPostProcessSettings
{
	TMap<FName, float> ScalarParameter;
	TMap<FName, FVector> VectorParameter;
	TMap<FName, FLinearColor> ColorParameter;

	struct FMaterialParameters
	{
		TMap<FName, float> ScalarParameter;
		TMap<FName, FVector> VectorParameter;
		TMap<FName, FLinearColor> ColorParameter;
	};

	struct FMaterialEntry
	{
		FString MaterialPath;
		float BlendWeight = 1.0f;
		FMaterialParameters Parameters;
	};

	TArray<FMaterialEntry> Materials;

	float GetScalar(FName Name, float DefaultValue = 0.0f) const; 
	void SetScalar(FName Name, float Value);
	FVector GetVector(FName Name, FVector DefaultValue= FVector(0.f,0.f,0.f)) const;
	void SetVector(FName Name, FVector Value);
	FLinearColor GetColor(FName Name, FLinearColor DefaultValue= FLinearColor(0.f, 0.f, 0.f, 1.f)) const;
	void SetColor(FName Name, FLinearColor Value);

	FMaterialEntry& FindOrAddMaterial(const FString& MaterialPath);
	void AddMaterial(const FString& MaterialPath, float BlendWeight = 1.0f);
	void SetMaterialScalar(const FString& MaterialPath, FName Name, float Value);
	float GetMaterialScalar(const FString& MaterialPath, FName Name, float DefaultValue = 0.0f) const;
};
struct FMinimalViewInfo
{
	FVector Location;
	FRotator Rotation;
	float FOV;
	float NearZ;
	float FarZ;
	bool bIsOrthogonal;
	float OrthoWidth;
	
	FPostProcessSettings PostPorcessSettings;
	float PostProcessBlendWeight = 1.0f;
};

// Gette Setter Helper Functions
template<typename TMap, typename TValue>
TValue GetParameterValue(const TMap& Map, FName Name, TValue DefaultValue)
{
	auto it = Map.find(Name);
	if (it != Map.end())
	{
		return it->second;
	}
	return DefaultValue;
}

template<typename TMap, typename TValue>
void SetParameterValue(TMap& Map, FName Name, TValue Value)
{
	Map[Name] = Value;
}
