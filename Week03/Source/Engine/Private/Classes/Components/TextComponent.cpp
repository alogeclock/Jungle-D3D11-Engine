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
	bMeshDirty = true;
        SetText("asdfaaaaaaaaaaaaaaaaaaa");
}

UTextComponent::~UTextComponent() {}

void UTextComponent::Render(URenderer &renderer) {
    if (TextVertices.empty()) return;

    FVector<float> CamRight, CamUp, CamForward;
    if (renderer.GetCameraBasis(CamRight, CamUp, CamForward))
    {
        UpdateBillboard(CamForward, CamUp);
    }
    else
    {
        RTMatrix = GetWorldMatrix();
    }

    FConstants constants;
    constants.MVPMatrix = RTMatrix;
    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);

    renderer.RenderText(this, TextVertices, constants);
}

void UTextComponent::UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp)
{
    const FMatrix<float>& World = GetWorldMatrix();

    const FVector<float> WorldPos(World.M[3][0], World.M[3][1], World.M[3][2]);

    FVector<float> WorldScale;
    WorldScale.X = std::sqrt(World.M[0][0] * World.M[0][0] + World.M[0][1] * World.M[0][1] + World.M[0][2] * World.M[0][2]);
    WorldScale.Y = std::sqrt(World.M[1][0] * World.M[1][0] + World.M[1][1] * World.M[1][1] + World.M[1][2] * World.M[1][2]);
    WorldScale.Z = std::sqrt(World.M[2][0] * World.M[2][0] + World.M[2][1] * World.M[2][1] + World.M[2][2] * World.M[2][2]);

    FVector<float> FaceDir = InCameraForward * -1.0f; // 카메라를 향하도록 반전
    if (!FaceDir.Normalize())
        FaceDir = FVector<float>(1.f, 0.f, 0.f);

    FVector<float> Up = InWorldUp;
    if (!Up.Normalize())
        Up = FVector<float>(0.f, 0.f, 1.f);

    FVector<float> Right = FVector<float>::CrossProduct(Up, FaceDir);
    if (!Right.Normalize())
        Right = FVector<float>(0.f, 1.f, 0.f);

    Up = FVector<float>::CrossProduct(FaceDir, Right);
    Up.Normalize();

    const FMatrix<float> R(
        FPlane<float>(Right.X,   Right.Y,   Right.Z,   0.0f),
        FPlane<float>(Up.X,      Up.Y,      Up.Z,      0.0f),
        FPlane<float>(FaceDir.X, FaceDir.Y, FaceDir.Z, 0.0f),
        FPlane<float>(0.0f,      0.0f,      0.0f,      1.0f)
    );

    const FMatrix<float> S = FScaleMatrix<float>(WorldScale);
    const FMatrix<float> T = FTranslationMatrix<float>(WorldPos);
    RTMatrix = S * R * T;
}


