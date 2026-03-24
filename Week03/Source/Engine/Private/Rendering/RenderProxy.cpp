#include "Source/Engine/Public/Rendering/RenderProxy.h"
#include "Source/Engine/Public/Rendering/Renderer.h"

void FStaticMeshRenderProxy::GenerateRenderCommand(URenderer& Renderer)
{
    FRenderCommand Command;
    
    // 1. 커맨드 타입 지정
    Command.CommandType = ERenderCommandType::StaticPrimitive;
    
    // 2. 공통 렌더 스테이트 복사
    Command.Constants = this->Constants;
    Command.ConstantsColor = this->ConstantsColor;
    Command.Topology = this->Topology;
    Command.CullMode = this->CullMode;
    Command.bEnableDepthTest = this->bEnableDepthTest;
    
    // 3. Static 전용 데이터 세팅
    Command.PrimitiveType = this->PrimitiveType;
    
    // 4. 렌더러의 큐에 커맨드 제출 (Renderer 쪽에 해당 함수가 구현되어 있어야 함)
    Renderer.SubmitCommand(Command); 
}

void FDynamicMeshRenderProxy::GenerateRenderCommand(URenderer& Renderer)
{
    FRenderCommand Command;
    
    // 1. 커맨드 타입 지정
    Command.CommandType = ERenderCommandType::DynamicPrimitive;
    
    // 2. 공통 렌더 스테이트 복사
    Command.Constants = this->Constants;
    Command.ConstantsColor = this->ConstantsColor;
    Command.Topology = this->Topology;
    Command.CullMode = this->CullMode;
    Command.bEnableDepthTest = this->bEnableDepthTest;
    
    // 3. Dynamic 전용 데이터 세팅 (런타임 버퍼 정보)
    Command.VertexBuffer = this->VertexBuffer;
    Command.IndexBuffer = this->IndexBuffer;
    Command.NumIndices = this->NumIndices;
    Command.Stride = this->Stride;
    
    // 4. 렌더러의 큐에 커맨드 제출
    Renderer.SubmitCommand(Command);
}