#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

struct FTextGpuBuffer;

struct FTransform;
class URenderer;

class UBillboardComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(UBillboardComponent, UPrimitiveComponent)

    UBillboardComponent(const FString& InString);
    virtual ~UBillboardComponent() override;

    void ApplyBillboardTransform(const FTransform& TargetTransform,
                                 const FVector<float>& CameraRotation,
                                 const FMatrix<float>& ViewMatrix,
                                 float FOV,
                                 bool bIsOrthogonal,
                                 const FVector<float>& CameraLocation,
                                 const FVector<float>& CameraLookAt);

    virtual void Render(URenderer& renderer) override;
    virtual void Submit() override;

    void SetTexturePath(const FString& InTexturePath) { TexturePath = InTexturePath; }
    const FString& GetTexturePath() const { return TexturePath; }

private:
    float CalculateScaleFactor(const FTransform& TargetTransform,
                               const FMatrix<float>& ViewMatrix,
                               float FOV,
                               bool bIsOrthogonal,
                               const FVector<float>& CameraLocation,
                               const FVector<float>& CameraLookAt) const;

private:
    FString TexturePath = "Data/Texture/EmptyActor.dds";
    TArray<FTextureVertex> BillboardVertices;
    TArray<uint32> BillboardIndices;

    FTextGpuBuffer* GpuBuffers = nullptr;
};
