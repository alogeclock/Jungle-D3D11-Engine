#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

UPrimitiveComponent::~UPrimitiveComponent() {}

void UPrimitiveComponent::Render(URenderer &renderer)
{
    FConstants constants;
    constants.worldMatrix = Transform.ToMatrix() * ParentMatrix;
    renderer.SetDepthTestEnable(bEnableDepthTest);

	FConstantsColor constantsColor;
    constantsColor.a = ColorAlpha;

    renderer.RenderPrimitive(this, constants, constantsColor);
}

void UPrimitiveComponent::Selected() { ColorAlpha = 0.5f; }

void UPrimitiveComponent::NotSelected() { ColorAlpha = 1.0f; }

FHitResult UPrimitiveComponent::IntersectRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection) 
{ 
    FHitResult Result;

    if (PrimitiveType == EPrimitiveType::None)
        return Result;

    TArray<FVertex>* vertices = UMeshManager::Get().GetVertexData(PrimitiveType);
    uint32 NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);

    // World Matrix로 Vertex를 World Space로 변환
    FMatrix<float> WorldMatrix = GetWorldMatrix(); // TRS 행렬

    // Index 없이 Vertex 3개씩 = Triangle 1개
    for (uint32 i = 0; i + 2 < NumVertices; i += 3)
    {
        FVector4<float> V0_L = FVector4<float>(vertices->at(i).Position.X, vertices->at(i).Position.Y, vertices->at(i).Position.Z, 1.0f);
        FVector4<float> V1_L = FVector4<float>(vertices->at(i + 1).Position.X, vertices->at(i + 1).Position.Y, vertices->at(i + 1).Position.Z, 1.0f);
        FVector4<float> V2_L = FVector4<float>(vertices->at(i + 2).Position.X, vertices->at(i + 2).Position.Y, vertices->at(i + 2).Position.Z, 1.0f);

        FVector4<float> V0_W = FVector4<float>(V0_L * WorldMatrix);
        FVector4<float> V1_W = FVector4<float>(V1_L * WorldMatrix);
        FVector4<float> V2_W = FVector4<float>(V2_L * WorldMatrix);

        FVector<float> V0 = FVector<float>(V0_W.X, V0_W.Y, V0_W.Z);
        FVector<float> V1 = FVector<float>(V1_W.X, V1_W.Y, V1_W.Z);
        FVector<float> V2 = FVector<float>(V2_W.X, V2_W.Y, V2_W.Z);

        float T = 0.f;
        if (RayIntersectsTriangle(RayOrigin, RayDirection, V0, V1, V2, T))
        {
            // 교차점 중 가장 가까운 것만 저장
            if (T < Result.Distance)
            {
                Result.bHit = true;
                Result.Distance = T;
                Result.HitPoint = RayOrigin + RayDirection * T;
                Result.HitComponent = this;
            }
        }
    }

    return Result;
}
