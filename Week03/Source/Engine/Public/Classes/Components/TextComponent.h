#pragma once
#include "PrimitiveComponent.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Vector.h"

class UTextComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT(UTextComponent, UPrimitiveComponent)
  public:
    UTextComponent(const FString &InString);
    virtual ~UTextComponent() override;
    FString        FilePath;
    const FString &GetText() const { return Text; }
    //FString        FilePath = "Data/Texture/DejaVu Sans Mono.dds";
    // UUID
    void SetText(const uint32 UUID);
    void SetText(const FString &InText)
    {
        if (Text == InText)
            return;
        Text = InText;
        bMeshDirty = true;
    }

    virtual void UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp = FVector<float>(0, 0, 1));

    virtual void Render(URenderer &renderer);
    virtual void  RebuildMesh();

  protected:
    FString        Text = FString("Text");
    FMatrix<float> RTMatrix;
    bool           bMeshDirty = true;

    TArray<FTextVertex> TextVertices;
};
