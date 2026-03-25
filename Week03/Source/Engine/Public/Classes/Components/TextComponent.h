#pragma once
#include "PrimitiveComponent.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Vector.h"

class UTextComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT_START(UTextComponent, UPrimitiveComponent)
    DECLARE_END 
public :
    UTextComponent(const FString &InString);
    virtual ~UTextComponent() override;

    virtual void UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp = FVector<float>(0, 0, 1));

    virtual void Render(URenderer &renderer);
    virtual void RebuildMesh();

    virtual void Submit(const FSceneViewOptions& ViewOptions) override;
    
    const FString &GetText() const { return Text; }
    const FString &GetFilePath() const { return FilePath; }
    const FString &GetKoreanFilePath() const { return FilePathKor; }
 
    void SetText(const uint32 UUID);
    void SetText(const FString &InText)
    {
        if (Text == InText)
            return;
        Text = InText;
        bMeshDirty = true;
    }
    void SetFilePath(const FString& InFilePath) { FilePath = InFilePath; }
    void SetKoreanFilePath(const FString& InFilePath) { FilePathKor = InFilePath; }

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

    uint32        VertexBufferSize = 0; // 현재 버퍼 용량
    uint32        IndexBufferSize = 0;

};
