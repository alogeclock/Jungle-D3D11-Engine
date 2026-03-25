#include "Source/Engine/Public/Classes/Components/TextComponent.h"
#include <Source/Engine/Public/Classes/TextMeshBuilder.h>
#include "Source/Core/Public/Math/ScaleMatrix.h"
#include "Source/Core/Public/Math/TranslationMatrix.h"
#include "World.h"
#include <cmath>
#include <iostream>

UTextComponent::UTextComponent(const FString &InString) : UPrimitiveComponent(InString)
{
	PrimitiveType = EPrimitiveType::Text;
	CullMode = ECullMode::None;      // 텍스트가 카메라 방향에 따라 통째로 컬링되는 상황 방지
	bEnableDepthTest = false;        // 기본적으로 항상 보이게
    bVIsible = true;
    FilePath = "Data/Texture/KorName.png";
}

UTextComponent::~UTextComponent() {

}

void UTextComponent::SetText(const uint32 UUID){
		char buf[64];
		sprintf_s(buf, "UID:%u", UUID);
		Text = FString(buf);
		bMeshDirty = true;
}

FHitResult UTextComponent::IntersectRayMeshTriangle(const FVector<float>& RayOrigin, const FVector<float>& RayDirection)
{
    FHitResult Result;
    if (PrimitiveType == EPrimitiveType::UUID) return Result;

    // World Matrix로 Vertex를 World Space로 변환
    FMatrix<float> WorldMatrix = GetWorldMatrix(); // TRS 행렬
    uint32 NumIndices = static_cast<uint32>(TextIndeices.size());
    
    if (NumIndices > 0)
    {
        // Index buffer 있는 경우
        for (uint32 i = 0; i + 2 < NumIndices; i += 3)
        {
            // index로 vertex 참조
            const FTextureVertex &v0 = TextVertices.at(TextIndeices.at(i));
            const FTextureVertex &v1 = TextVertices.at(TextIndeices.at(i + 1));
            const FTextureVertex &v2 = TextVertices.at(TextIndeices.at(i + 2));

            FVector4<float> V0_L = {v0.Position.X, v0.Position.Y, v0.Position.Z, 1.f};
            FVector4<float> V1_L = {v1.Position.X, v1.Position.Y, v1.Position.Z, 1.f};
            FVector4<float> V2_L = {v2.Position.X, v2.Position.Y, v2.Position.Z, 1.f};

            FVector4<float> V0_W = V0_L * WorldMatrix;
            FVector4<float> V1_W = V1_L * WorldMatrix;
            FVector4<float> V2_W = V2_L * WorldMatrix;

            FVector<float> V0 = {V0_W.X, V0_W.Y, V0_W.Z};
            FVector<float> V1 = {V1_W.X, V1_W.Y, V1_W.Z};
            FVector<float> V2 = {V2_W.X, V2_W.Y, V2_W.Z};

            float T = 0.f;
            if (RayIntersectsTriangle(RayOrigin, RayDirection, V0, V1, V2, T))
            {
                if (T < Result.Distance)
                {
                    Result.bHit = true;
                    Result.Distance = T;
                    Result.HitPoint = RayOrigin + RayDirection * T;
                    Result.HitComponent = this;
                }
            }
        }
    }

    return Result;
}


void UTextComponent::Render(URenderer &renderer)
{
}

void UTextComponent::RebuildMesh()
{
    FTextMeshBuilder::BuildTextMesh(Text, &TextVertices, &TextIndeices);
    LocalAABB = UMeshManager::Get().ComputeAABB(TextVertices);
    bLocalBoundsDirty = false;
    UpdateBounds();
    bMeshDirty   = false;
}

void UTextComponent::UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp) {}

// Text Batcher로 전송되므로 추가적인 Submit 작업을 하지 않는다.
void UTextComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    if (Text.empty())
    {
        return;
    }

    if (bMeshDirty)
    {
        RebuildMesh();
    }

    UPrimitiveComponent::Submit(ViewOptions);

    UTextBatcherComponent* TextBatcher = GWorld->GetTextBatcherComponent();
    if (TextBatcher == nullptr)
    {
        return;
    }

    TextBatcher->Submit(FilePath, Text, GetWorldMatrix(), EEngineShowFlags::SF_Primitives);

    bool bGlobalDrawAABB = (ViewOptions.ShowFlags & EEngineShowFlags::SF_AABB) != EEngineShowFlags::None;
    if (bShowAABB && bGlobalDrawAABB && GWorld && GWorld->GetLineBatcherComponent())
    {
        UpdateBounds(); 
        GWorld->GetLineBatcherComponent()->DrawBox(WorldAABB, {0.3f, 1.0f, 0.3f, 1.0f});
    }
}
