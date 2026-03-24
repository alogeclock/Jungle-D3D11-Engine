#include "Source/Engine/Public/Classes/Components/RingComponent.h"

URingComponent::URingComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Ring;
    bEnableDepthTest = false;
}

URingComponent::~URingComponent() {}

void URingComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    UPrimitiveComponent::Submit(ViewOptions);
    FRenderCommand &Command = RenderProxy->RenderCommand;

    Command.VertexBuffer = UMeshManager::Get().GetVertexBuffer(PrimitiveType);
    Command.IndexBuffer = UMeshManager::Get().GetIndexBuffer(PrimitiveType);
    Command.NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);
    Command.NumIndices = UMeshManager::Get().GetNumIndices(PrimitiveType);
    Command.Stride = sizeof(FVertex);
}