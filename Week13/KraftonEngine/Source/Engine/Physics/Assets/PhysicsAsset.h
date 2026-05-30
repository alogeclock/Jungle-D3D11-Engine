#pragma once

#include "PhysicsBodySetup.h"
#include "PhysicsConstraintSetup.h"

/** SkeletalMesh에 적용되는 Physics Asset */
class UPhysicsAsset : public UObject
{
public:
    UPhysicsAsset()  = default;
    virtual ~UPhysicsAsset() = default;

    const TArray<UPhysicsBodySetup*>& GetBodySetups() const { return BodySetups; }
    TArray<UPhysicsBodySetup*>& GetMutableBodySetups() { return BodySetups; }
    void SetBodySetups(const TArray<UPhysicsBodySetup*>& InBodySetups) { BodySetups = InBodySetups; }
    void AddBodySetup(UPhysicsBodySetup* InBodySetup) { BodySetups.push_back(InBodySetup); }

    const TArray<FPhysicsConstraintSetup>& GetConstraintSetups() const { return ConstraintSetups; }
    TArray<FPhysicsConstraintSetup>& GetMutableConstraintSetups() { return ConstraintSetups; }
    void SetConstraintSetups(const TArray<FPhysicsConstraintSetup>& InConstraintSetups) { ConstraintSetups = InConstraintSetups; }
    void AddConstraintSetup(const FPhysicsConstraintSetup& InConstraintSetup) { ConstraintSetups.push_back(InConstraintSetup); }

    UPhysicsBodySetup* FindBodySetupByTargetBoneName(const FName& TargetBoneName) const
    {
        for (UPhysicsBodySetup* BodySetup : BodySetups)
        {
            if (BodySetup && BodySetup->GetTargetBoneName() == TargetBoneName)
                return BodySetup;
        }
        return nullptr;
    }

private:
    TArray<UPhysicsBodySetup*>      BodySetups;
    TArray<FPhysicsConstraintSetup> ConstraintSetups;
};
