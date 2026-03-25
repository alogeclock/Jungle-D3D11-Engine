#include "Source/Engine/Public/Classes/Components/CubeComponent.h"

UCubeComponent::UCubeComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Cube;
}

UCubeComponent::~UCubeComponent() {}

// Tick 이후 갱신 타이밍에 호출됨
void UCubeComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    UPrimitiveComponent::Submit(ViewOptions);
    FRenderCommand &Command = RenderProxy->RenderCommand;
    
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
FRenderProxy* UCubeComponent::CreateRenderProxy()
{
    FStaticMeshRenderProxy* Proxy = new FStaticMeshRenderProxy();
    Proxy->PrimitiveType = EPrimitiveType::Cube;
    Proxy->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    return Proxy;
}
*/