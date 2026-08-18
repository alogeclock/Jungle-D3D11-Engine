#include "Source/Core/Public/Memory.h"
#include "Source/Engine/Public/Classes/Components/SphereComponent.h"

// 개별 USphereComponent에서 Topology, NumVertices를 결정해야 한다. 
USphereComponent::USphereComponent(const FString &InString, float inSphereRadius) : UPrimitiveComponent(InString)
{
    PrimitiveType = EPrimitiveType::Sphere;
	SetScale({inSphereRadius, inSphereRadius, inSphereRadius}); 
}

USphereComponent::~USphereComponent() {}

void USphereComponent::Submit(const FSceneViewOptions& ViewOptions)
{
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