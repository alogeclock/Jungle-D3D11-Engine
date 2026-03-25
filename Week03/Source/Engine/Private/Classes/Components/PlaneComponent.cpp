#include "Source/Engine/Public/Classes/Components/PlaneComponent.h"

UPlaneComponent::UPlaneComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Plane;
}

UPlaneComponent::~UPlaneComponent() {}

void UPlaneComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    if (!RenderProxy) return;

    UPrimitiveComponent::Submit(ViewOptions);
    FRenderCommand &Command = RenderProxy->RenderCommand;
    
    Command.bIsTextured = false;
    // 정적 Vertex Buffer의 경우 처음 한 번 외에는 호출하지 않음
    if (Command.VertexBuffer == nullptr)
    {
        Command.VertexBuffer = UMeshManager::Get().GetVertexBuffer(PrimitiveType);
        Command.IndexBuffer = UMeshManager::Get().GetIndexBuffer(PrimitiveType);
        Command.NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);
        Command.NumIndices = UMeshManager::Get().GetNumIndices(PrimitiveType);
        Command.Stride = sizeof(FVertex);
    }
}