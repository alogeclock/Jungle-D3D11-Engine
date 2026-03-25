#include "Source/Engine/Public/Classes/Components/CubeArrowComponent.h"

UCubeArrowComponent::UCubeArrowComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::CubeArrow;
    bEnableDepthTest = false;
}

UCubeArrowComponent::~UCubeArrowComponent() {}

// Tick 이후 갱신 타이밍에 호출됨
void UCubeArrowComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    UPrimitiveComponent::Submit(ViewOptions);
    FRenderCommand &Command = RenderProxy->RenderCommand;
    
    // 정적 Vertex Buffer의 경우 처음 한 번 외에는 호출하지 않음
    Command.bIsTextured = false;
    if (Command.VertexBuffer == nullptr)
    {
        Command.VertexBuffer = UMeshManager::Get().GetVertexBuffer(PrimitiveType);
        Command.IndexBuffer = UMeshManager::Get().GetIndexBuffer(PrimitiveType);
        Command.NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);
        Command.NumIndices = UMeshManager::Get().GetNumIndices(PrimitiveType);
        Command.Stride = sizeof(FVertex);
    }
}

/*
FRenderProxy* UCubeArrowComponent::CreateRenderProxy()
{
    FStaticMeshRenderProxy* Proxy = new FStaticMeshRenderProxy();
    Proxy->PrimitiveType = EPrimitiveType::Cube;
    Proxy->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    return Proxy;
}
*/