#include "Physics/Common/PhysicsMaterialTypes.h"

IMPLEMENT_CLASS(UPhysicalMaterial, UObject)

void UPhysicalMaterial::Serialize(FArchive& Ar)
{
	constexpr uint32 VersionTag = 0x504D4154u;
	constexpr uint32 Version = 1u;

	uint32 SerializedTag = VersionTag;
	uint32 SerializedVersion = Version;
	if (Ar.IsSaving())
	{
		Ar << SerializedTag;
		Ar << SerializedVersion;
	}
	else
	{
		Ar << SerializedTag;
		if (SerializedTag == VersionTag)
		{
			Ar << SerializedVersion;
		}
		else
		{
			SerializedVersion = 0u;
		}
	}

	uint8 FrictionMode = static_cast<uint8>(FrictionCombineMode);
	uint8 RestitutionMode = static_cast<uint8>(RestitutionCombineMode);
	uint8 SoftMode = static_cast<uint8>(SoftCollisionMode);

	Ar << Friction;
	Ar << StaticFriction;
	Ar << FrictionMode;
	Ar << bOverrideFrictionCombineMode;
	Ar << Restitution;
	Ar << RestitutionMode;
	Ar << bOverrideRestitutionCombineMode;
	Ar << Density;
	Ar << SleepLinearVelocityThreshold;
	Ar << SleepAngularVelocityThreshold;
	Ar << SleepCounterThreshold;
	Ar << RaiseMassToPower;
	Ar << SurfaceType;
	Ar << Strength;
	Ar << DamageModifier;
	Ar << bShowExperimentalProperties;
	Ar << SoftMode;
	Ar << SoftCollisionThickness;
	Ar << BaseFrictionImpulse;

	if (Ar.IsLoading())
	{
		FrictionCombineMode = static_cast<EPhysicsMaterialCombineMode>(FrictionMode);
		RestitutionCombineMode = static_cast<EPhysicsMaterialCombineMode>(RestitutionMode);
		SoftCollisionMode = static_cast<EPhysicsMaterialSoftCollisionMode>(SoftMode);
	}
}

