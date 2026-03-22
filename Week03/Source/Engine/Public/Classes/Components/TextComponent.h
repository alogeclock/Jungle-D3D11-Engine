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

    virtual void UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp = FVector<float>(0, 0, 1));

    virtual void Render(URenderer &renderer);
    virtual void  RebuildMesh();
    
    const FString &GetText() const { return Text; }
 
    void SetText(const uint32 UUID);
    void SetText(const FString &InText)
    {
        if (Text == InText)
            return;
        Text = InText;
        bMeshDirty = true;
    }

  protected:
    FString        FilePath;
    FString        FilePathKor;
    FString        Text = FString("Text");
    FMatrix<float> RTMatrix;
    bool           bMeshDirty = true;
    bool bVIsible = false;

	TArray<FTextVertex> TextVertices;
    ID3D11Buffer* VertexBuffer     = nullptr;
    uint32        VertexBufferSize = 0; // 현재 버퍼 용량
};
