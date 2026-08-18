#pragma once

#include "Object/Object.h"
#include "Object/FName.h"
#include "PhysicsShapeSetup.h"
#include "PhysicsBodySetup.generated.h"

/**
 * @file PhysicsBodySetup.h
 * @brief Bone 또는 Component에 대응되는 Physics Body 설정 정의.
 */

/** Body 충돌 복잡도 설정 */
enum class EPhysicsBodyCollisionComplexity : uint8
{
	ProjectDefault = 0,
	UseSimpleAsComplex,
	UseComplexAsSimple,
};

/** Bone 또는 Component에 대응되는 Physics Body 설정 */
UCLASS()
class UPhysicsBodySetup : public UObject
{
public:
    GENERATED_BODY(UPhysicsBodySetup)

    UPhysicsBodySetup() = default;
    virtual ~UPhysicsBodySetup() = default;

    const FName& GetTargetBoneName() const { return TargetBoneName; }
    void SetTargetBoneName(const FName& InName) { TargetBoneName = InName; }

    const FPhysicsAggregateShapeSetup& GetShapeSetup() const { return ShapeSetup; }
    FPhysicsAggregateShapeSetup& GetMutableShapeSetup() { return ShapeSetup; }
    void SetShapeSetup(const FPhysicsAggregateShapeSetup& InShapeSetup) { ShapeSetup = InShapeSetup; }

    EPhysicsBodyType GetBodyType() const { return BodyType; }
    void SetBodyType(EPhysicsBodyType InBodyType) { BodyType = InBodyType; }

    float GetMass() const { return Mass; }
    void SetMass(float InMass) { Mass = InMass; }

    float GetLinearDamping() const { return LinearDamping; }
    void SetLinearDamping(float InLinearDamping) { LinearDamping = InLinearDamping; }

    float GetAngularDamping() const { return AngularDamping; }
    void SetAngularDamping(float InAngularDamping) { AngularDamping = InAngularDamping; }

    EPhysicsCollisionEnabled GetCollisionEnabled() const { return CollisionDesc.CollisionEnabled; }
    void SetCollisionEnabled(EPhysicsCollisionEnabled InEnabled) { CollisionDesc.CollisionEnabled = InEnabled; }

    const FPhysicsCollisionDesc& GetCollisionDesc() const { return CollisionDesc; }
    void SetCollisionDesc(const FPhysicsCollisionDesc& InCollisionDesc) { CollisionDesc = InCollisionDesc; }

    FPhysicsBodyDesc BuildBodyDesc() const
    {
        FPhysicsBodyDesc Desc;
        Desc.BodyType       = BodyType;
        Desc.Mass           = Mass;
        Desc.LinearDamping  = LinearDamping;
        Desc.AngularDamping = AngularDamping;
        Desc.CollisionDesc  = CollisionDesc;
        Desc.Shapes         = ShapeSetup.BuildShapeDescs();
        return Desc;
    }

    void Serialize(FArchive& Ar) override;

public:
    bool                           bOverrideMass                  = false;
    bool                           bEnableGravity                 = true;
    int32                          GravityGroupIndex              = 0;
    bool                           bUpdateKinematicFromSimulation = false;
    bool                           bGyroscopicTorqueEnabled       = false;
    bool                           bDoubleSidedGeometry           = false;
    UPhysicalMaterial*             SimpleCollisionPhysicalMaterial = nullptr;
    UPhysicalMaterial*             PhysMaterialOverride            = nullptr;
    bool                           bSkipScaleFromAnimation        = false;
    bool                           bConsiderForBounds             = true;
    bool                           bSimulationGeneratesHitEvents  = false;
    bool                           bNeverNeedsCookedCollisionData = false;
    EPhysicsBodyCollisionComplexity CollisionComplexity           = EPhysicsBodyCollisionComplexity::UseSimpleAsComplex;

private:
    FName                       TargetBoneName = FName::None;
    FPhysicsAggregateShapeSetup ShapeSetup;
    EPhysicsBodyType            BodyType       = EPhysicsBodyType::PBT_Dynamic;
    float                       Mass           = 1.0f;
    float                       LinearDamping  = 0.0f;
    float                       AngularDamping = 0.05f;
    FPhysicsCollisionDesc       CollisionDesc;
};
