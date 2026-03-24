#include "Source/Engine/Public/Classes/Components/UUIDTextComponent.h"
#include "Source/Engine/Public/Classes/Components/TextBatcherComponent.h"
#include "World.h"

UUUIDTextComponent::UUUIDTextComponent(const FString &InString)
    : UTextComponent(InString)
{
	PrimitiveType = EPrimitiveType::UUID;
	CullMode = ECullMode::None;      // 텍스트가 카메라 방향에 따라 통째로 컬링되는 상황 방지
	bEnableDepthTest = false;        // 기본적으로 항상 보이게
    bShowUUID = false;
    FilePath = "Data/Texture/DejaVu Sans Mono.dds";
}

UUUIDTextComponent::~UUUIDTextComponent() {}

void UUUIDTextComponent::Render(URenderer &renderer) {
	 if (!IsVisible() || !bShowUUID)
    {
        return;
    }

    if (Text.empty())
    {
        return;
    }

    if (!renderer.CheckShowFlag(EEngineShowFlags::SF_UUID))
    {
        return;
    }

    FVector<float> ForwardVector;
    FVector<float> RightVector;
    FVector<float> UpVector;

    if (renderer.GetCameraBasis(RightVector, UpVector, ForwardVector))
    {
        UpdateBillboard(ForwardVector, UpVector);
    }
    else
    {
        RTMatrix = GetWorldMatrix();
    }

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    UTextBatcherComponent* TextBatcher = World->GetTextBatcherComponent();
    if (TextBatcher == nullptr)
    {
        return;
    }

    TextBatcher->Submit(FilePath, Text, RTMatrix);
}

void UUUIDTextComponent::UpdateBillboard(const FVector<float> &InCameraForward, const FVector<float> &InWorldUp)
{
    const FMatrix<float> &World = GetWorldMatrix();

    const FVector<float> WorldPos(World.M[3][0], World.M[3][1], World.M[3][2]+Zoffset);

    FVector<float> WorldScale;
    WorldScale.X = std::sqrt(World.M[0][0] * World.M[0][0] + World.M[0][1] * World.M[0][1] + World.M[0][2] * World.M[0][2]);
    WorldScale.Y = std::sqrt(World.M[1][0] * World.M[1][0] + World.M[1][1] * World.M[1][1] + World.M[1][2] * World.M[1][2]);
    WorldScale.Z = std::sqrt(World.M[2][0] * World.M[2][0] + World.M[2][1] * World.M[2][1] + World.M[2][2] * World.M[2][2]);

    FVector<float> FaceDir = InCameraForward * -1.0f; // 카메라를 향하도록 반전
    if (!FaceDir.Normalize())
        FaceDir = FVector<float>(1.f, 0.f, 0.f);

    FVector<float> Up = InWorldUp* -1.0f;
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

    const FMatrix<float> S = FScaleMatrix<float>::Identity();
    const FMatrix<float> T = FTranslationMatrix<float>(WorldPos);
    RTMatrix = S * R * T;
}

void UUUIDTextComponent::Submit()
{
    if (!RenderProxy) return;

    // 캐스팅 후 정보 갱신 (메모리 복사 비용 최소화)
    FStaticMeshRenderProxy* StaticProxy = static_cast<FStaticMeshRenderProxy*>(RenderProxy);
    
    // 컴포넌트의 최신 정보를 Proxy로 전달 (상태 갱신)
    StaticProxy->Constants.MVPMatrix = GetWorldMatrix();
    StaticProxy->ConstantsColor = {Color.X, Color.Y, Color.Z, Color.W};
    StaticProxy->CullMode = this->CullMode;
    StaticProxy->bEnableDepthTest = this->bEnableDepthTest;
}

FRenderProxy* UUUIDTextComponent::CreateRenderProxy()
{
    FStaticMeshRenderProxy* Proxy = new FStaticMeshRenderProxy();
    Proxy->PrimitiveType = EPrimitiveType::Cube;
    Proxy->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    return Proxy;
}