#pragma once

#include "Math/Vector.h"
#include "Object/Object.h"
#include "PhysicsBodySetup.h"
#include "PhysicsConstraintSetup.h"
#include "PhysicsAsset.generated.h"

struct FPhysicsAssetGraphNodeLayout
{
    FString  NodeKey;
    FVector2 Position = FVector2(0.0f, 0.0f);
};

inline FArchive& operator<<(FArchive& Ar, FPhysicsAssetGraphNodeLayout& NodeLayout)
{
    Ar << NodeLayout.NodeKey;
    Ar << NodeLayout.Position.X;
    Ar << NodeLayout.Position.Y;
    return Ar;
}

struct FPhysicsAssetGraphViewState
{
    FVector2                             Pan = FVector2(32.0f, 32.0f);
    float                                Zoom = 1.0f;
    TArray<FPhysicsAssetGraphNodeLayout> NodeLayouts;
};

inline FArchive& operator<<(FArchive& Ar, FPhysicsAssetGraphViewState& ViewState)
{
    Ar << ViewState.Pan.X;
    Ar << ViewState.Pan.Y;
    Ar << ViewState.Zoom;
    SerializeArrayElements(Ar, ViewState.NodeLayouts);
    return Ar;
}

enum class EPhysicsAssetRagdollMode : uint8
{
    PerBody = 0,
    PxAggregate,
};

inline const char* PhysicsAssetRagdollModeToString(EPhysicsAssetRagdollMode Mode)
{
    switch (Mode)
    {
    case EPhysicsAssetRagdollMode::PerBody:
        return "Per-Body";
    case EPhysicsAssetRagdollMode::PxAggregate:
        return "PxAggregate";
    default:
        return "Per-Body";
    }
}

/** SkeletalMesh에 적용되는 Physics Asset */
UCLASS()
class UPhysicsAsset : public UObject
{
public:
    GENERATED_BODY(UPhysicsAsset)

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

    void SetAssetPathFileName(const FString& InPathFileName) { AssetPathFileName = InPathFileName; }
    const FString& GetAssetPathFileName() const override { return AssetPathFileName; }

    void SetPreviewSkeletalMeshPath(const FString& InPath) { PreviewSkeletalMeshPath = InPath; }
    const FString& GetPreviewSkeletalMeshPath() const { return PreviewSkeletalMeshPath; }

    void SetRagdollMode(EPhysicsAssetRagdollMode InMode) { RagdollMode = InMode; }
    EPhysicsAssetRagdollMode GetRagdollMode() const { return RagdollMode; }

    const FPhysicsAssetGraphViewState& GetGraphViewState() const { return GraphViewState; }
    FPhysicsAssetGraphViewState& GetMutableGraphViewState() { return GraphViewState; }

    void Serialize(FArchive& Ar) override;

private:
    TArray<UPhysicsBodySetup*>      BodySetups;
    TArray<FPhysicsConstraintSetup> ConstraintSetups;
    FString                         AssetPathFileName = "None";
    FString                         PreviewSkeletalMeshPath;
    EPhysicsAssetRagdollMode        RagdollMode = EPhysicsAssetRagdollMode::PerBody;
    FPhysicsAssetGraphViewState     GraphViewState;
};
