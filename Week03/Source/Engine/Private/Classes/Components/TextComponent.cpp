#include "Source/Engine/Public/Classes/Components/TextComponent.h"
#include <Source/Engine/Public/Classes/TextMeshBuilder.h>
#include "Source/Core/Public/Math/ScaleMatrix.h"
#include "Source/Core/Public/Math/TranslationMatrix.h"
#include <cmath>
#include <iostream>

UTextComponent::UTextComponent(const FString &InString)
    : UPrimitiveComponent(InString)
{
	PrimitiveType = EPrimitiveType::Text;
	CullMode = ECullMode::None;      // 텍스트가 카메라 방향에 따라 통째로 컬링되는 상황 방지
	bEnableDepthTest = false;        // 기본적으로 항상 보이게
    bVIsible = true;
    FilePath = "Data/Texture/KorName.png";
}

UTextComponent::~UTextComponent() {

    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }
    if (IndexBuffer)
    {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }
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
            const FTextVertex &v0 = TextVertices.at(TextIndeices.at(i));
            const FTextVertex &v1 = TextVertices.at(TextIndeices.at(i + 1));
            const FTextVertex &v2 = TextVertices.at(TextIndeices.at(i + 2));

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
    if (bMeshDirty) RebuildMesh();
    if (TextVertices.empty()) return;

    FConstants constants;
    constants.MVPMatrix = GetWorldMatrix();

    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);

    renderer.RenderText(FilePath, constants, &TextVertices, &TextIndeices, &VertexBuffer,&IndexBuffer, VertexBufferSize, IndexBufferSize);
}

void UTextComponent::RebuildMesh()
{
    FTextMeshBuilder::BuildTextMesh(Text, &TextVertices, &TextIndeices);
    bMeshDirty   = false;
}

void UTextComponent::UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp) {}


