#pragma once
#include "PrimitiveComponent.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Vector.h"

class UTextComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT_START(UTextComponent, UPrimitiveComponent)
    PRIVATE_PROPERTY(UTextComponent, Text, String)
    DECLARE_END 
public :
    UTextComponent(const FString &InString);
    virtual ~UTextComponent() override;

    virtual void UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp = FVector<float>(0, 0, 1));

    virtual void Render(URenderer &renderer);
    virtual void RebuildMesh();

    virtual void Submit() override;
    virtual FRenderProxy* CreateRenderProxy() override;
    
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
    virtual FHitResult IntersectRayMeshTriangle(const FVector<float> &RayOrigin, const FVector<float> &RayDirection) override;

    FString        FilePath;
    FString        FilePathKor;
    FString        Text;
    FMatrix<float> RTMatrix;
    bool           bMeshDirty = true;
    bool bVIsible = false;

	TArray<FTextureVertex> TextVertices;
	TArray<uint32> TextIndeices;

    ID3D11Buffer* VertexBuffer     = nullptr;
    ID3D11Buffer* IndexBuffer     = nullptr;
    uint32        VertexBufferSize = 0; // 현재 버퍼 용량
    uint32        IndexBufferSize = 0;

};
