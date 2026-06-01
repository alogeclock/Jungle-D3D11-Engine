#include "PhysicsBodySetup.h"

#include "Serialization/Archive.h"

void UPhysicsBodySetup::Serialize(FArchive& Ar)
{
    Ar << TargetBoneName;
    Ar << ShapeSetup;

    uint8 SerializedBodyType = static_cast<uint8>(BodyType);
    Ar << SerializedBodyType;
    Ar << Mass;
    Ar << LinearDamping;
    Ar << AngularDamping;
    Ar << CollisionDesc;

    if (Ar.IsLoading())
    {
        BodyType = static_cast<EPhysicsBodyType>(SerializedBodyType);
    }
}
