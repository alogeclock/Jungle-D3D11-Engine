#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

struct FTextGpuBuffer;

struct FTransform;
struct FViewportCameraTransform;
class URenderer;

class UBillboardComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(UBillboardComponent, UPrimitiveComponent)

    UBillboardComponent(const FString& InString) : UPrimitiveComponent(InString)
    {
        PrimitiveType = EPrimitiveType::Billboard;
        TexturePath = "Data/Texture/DefaultBillboard.dds";
    }
    virtual ~UBillboardComponent() override {}

    void SetTexturePath(const FString& InTexturePath) { TexturePath = InTexturePath; }
    const FString& GetTexturePath() const { return TexturePath; }

    virtual void Submit(const FSceneViewOptions& ViewOptions) override;

    void ApplyBillboardTransform(const FTransform& TargetTransform, FViewportCameraTransform& Camera);

private:
    FMatrix<float> BuildBillboardWorldMatrix();

    FString TexturePath;

    FVector<float> CachedCameraRight = FVector<float>(1.0f, 0.0f, 0.0f);
    FVector<float> CachedCameraUp = FVector<float>(0.0f, 1.0f, 0.0f);
    FVector<float> CachedCameraForward = FVector<float>(0.0f, 0.0f, 1.0f);
};
