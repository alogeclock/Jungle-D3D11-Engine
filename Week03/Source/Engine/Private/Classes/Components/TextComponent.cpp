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
    FilePath = "Data/Texture/DejaVu Sans Mono.dds";
}

UTextComponent::~UTextComponent() {

    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }
}

void UTextComponent::SetText(const uint32 UUID){
		char buf[64];
		sprintf_s(buf, "UID:%u", UUID);
		Text = FString(buf);
		bMeshDirty = true;
	}


void UTextComponent::Render(URenderer &renderer)
{
    if (bMeshDirty) RebuildMesh();
    if (TextVertices.empty()) return;

    FConstants constants;
    constants.MVPMatrix = GetWorldMatrix();

    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);
    renderer.RenderText(FilePath, constants, &TextVertices, &VertexBuffer, VertexBufferSize);
}

void UTextComponent::RebuildMesh()
{
    TextVertices = FTextMeshBuilder::BuildTextMesh(Text);
    bMeshDirty   = false;
}

void UTextComponent::UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp) {}


