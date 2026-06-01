#include "PhysicsBodySetup.h"

#include "Serialization/Archive.h"

void UPhysicsBodySetup::Serialize(FArchive& Ar)
{
	constexpr uint32 VersionTag = 0x50425355u;
	constexpr uint32 Version = 2u;

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

	Ar << TargetBoneName;
	Ar << ShapeSetup;

	uint8 SerializedBodyType = static_cast<uint8>(BodyType);
	Ar << SerializedBodyType;
	Ar << Mass;
	Ar << LinearDamping;
	Ar << AngularDamping;
	Ar << CollisionDesc;

	if (SerializedVersion >= 2u || Ar.IsSaving())
	{
		uint8 SerializedCollisionComplexity = static_cast<uint8>(CollisionComplexity);
		Ar << bOverrideMass;
		Ar << bEnableGravity;
		Ar << GravityGroupIndex;
		Ar << bUpdateKinematicFromSimulation;
		Ar << bGyroscopicTorqueEnabled;
		Ar << bDoubleSidedGeometry;
		SerializePhysicalMaterialReference(Ar, SimpleCollisionPhysicalMaterial);
		SerializePhysicalMaterialReference(Ar, PhysMaterialOverride);
		Ar << bSkipScaleFromAnimation;
		Ar << bConsiderForBounds;
		Ar << bSimulationGeneratesHitEvents;
		Ar << bNeverNeedsCookedCollisionData;
		Ar << SerializedCollisionComplexity;

		if (Ar.IsLoading())
		{
			CollisionComplexity = static_cast<EPhysicsBodyCollisionComplexity>(SerializedCollisionComplexity);
		}
	}
	else if (Ar.IsLoading())
	{
		bOverrideMass = false;
		bEnableGravity = true;
		GravityGroupIndex = 0;
		bUpdateKinematicFromSimulation = false;
		bGyroscopicTorqueEnabled = false;
		bDoubleSidedGeometry = false;
		SimpleCollisionPhysicalMaterial = nullptr;
		PhysMaterialOverride = nullptr;
		bSkipScaleFromAnimation = false;
		bConsiderForBounds = true;
		bSimulationGeneratesHitEvents = false;
		bNeverNeedsCookedCollisionData = false;
		CollisionComplexity = EPhysicsBodyCollisionComplexity::UseSimpleAsComplex;
	}

	if (Ar.IsLoading())
	{
		BodyType = static_cast<EPhysicsBodyType>(SerializedBodyType);
	}
}
