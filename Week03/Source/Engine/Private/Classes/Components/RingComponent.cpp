#include "Source/Engine/Public/Classes/Components/RingComponent.h"

URingComponent::URingComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Ring;
}

URingComponent::~URingComponent() {}

void URingComponent::Submit()
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

FRenderProxy* URingComponent::CreateRenderProxy()
{
    FStaticMeshRenderProxy* Proxy = new FStaticMeshRenderProxy();
    Proxy->PrimitiveType = EPrimitiveType::Cube;
    Proxy->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    return Proxy;
}