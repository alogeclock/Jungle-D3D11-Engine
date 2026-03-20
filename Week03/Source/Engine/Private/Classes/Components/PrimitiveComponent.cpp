#include "Source/Core/Public/Memory.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include "Source/Engine/Object/Public/Actor.h"

UPrimitiveComponent::UPrimitiveComponent(const FString &InString) : USceneComponent(InString) {}

UPrimitiveComponent::~UPrimitiveComponent() {}

void UPrimitiveComponent::Render(URenderer &renderer)
{
    FConstants constants;
    constants.MVPMatrix = GetWorldMatrix();  // 부모 및 자신의 변경 사항을 반영한 GetWorldMatrix()를 호출한다.
    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);

    FConstantsColor constantsColor(Color.X, Color.Y, Color.Z, Color.W);

    renderer.RenderPrimitive(this, constants, constantsColor);
}

void UPrimitiveComponent::Selected() { SetColor({0.0f, 0.0f, 0.0f, 0.5f}); }

void UPrimitiveComponent::NotSelected() { SetColor({0.0f, 0.0f, 0.0f, 0.0f}); }

FHitResult UPrimitiveComponent::IntersectRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection)
{
    FHitResult Result;

    if (PrimitiveType == EPrimitiveType::None)
        return Result;

    switch (PrimitiveType)
    {
    // Vertex 적음 → 바로 Triangle 검사
    case EPrimitiveType::Triangle:
    case EPrimitiveType::Plane:
        Result = IntersectRayMeshTriangle(RayOrigin, RayDirection);
        break;

    // Vertex 많음 → Sphere -> AABB → Triangle  
    case EPrimitiveType::Cube:
    case EPrimitiveType::Sphere:
    case EPrimitiveType::Arrow:
    case EPrimitiveType::CubeArrow:
    case EPrimitiveType::Ring:
    default:
        if (!IntersectRayBoundingSphere(RayOrigin, RayDirection))
            return Result;
        if (!IntersectRayAABB(RayOrigin, RayDirection))
            return Result;
        Result = IntersectRayMeshTriangle(RayOrigin, RayDirection);
        break;
    }

    return Result;
}

FVector<float> UPrimitiveComponent::GetLocalAABBMin() const 
{ 
    switch (PrimitiveType)
    {
    case EPrimitiveType::Sphere:
        return FVector<float>(-1.0f, -1.0f, -1.0f);
    case EPrimitiveType::Cube:
        return FVector<float>(-0.5f, -0.5f, -0.5);
    default:
        return FVector<float>(-1.0f, -1.0f, -1.0f);
    }
}

FVector<float> UPrimitiveComponent::GetLocalAABBMax() const 
{
    switch (PrimitiveType)
    {
    case EPrimitiveType::Sphere:
        return FVector<float>(1.0f, 1.0f, 1.0f);
    case EPrimitiveType::Cube:
        return FVector<float>(0.5f, 0.5f, 0.5f);
    default:
        return FVector<float>(1.0f, 1.0f, 1.0f);
    }
}

FTransform UPrimitiveComponent::GetTransformFromOwner() const  
{  
    FTransform transform;

    if (GetOwner() == nullptr)
        return transform;
    
    return GetOwner()->GetRootComponent()->GetTransform();  
}

bool UPrimitiveComponent::IntersectRayBoundingSphere(const FVector<float> &RayOrigin, const FVector<float> &RayDirection)
{
    FTransform transform = GetTransformFromOwner();
    FVector<float> Center = transform.Location;

    float Max = transform.Scale.X > transform.Scale.Y ? transform.Scale.X : transform.Scale.Y;
    float Radius = Max > transform.Scale.Z ? Max : transform.Scale.Z;

    // ──────────────────────────────────────────────
    // Ray → Sphere 교차 판정 (기하학적 방법)
    //
    //  L = Center - RayOrigin  (Origin에서 구 중심까지 벡터)
    //  tca = L · RayDirection  (RayDirection으로의 정사영 길이)
    //  d²  = L·L - tca²        (Ray와 구 중심 사이의 최단거리²)
    //  d² <= r²  이면 교차
    // ──────────────────────────────────────────────
    const FVector<float> L = Center - RayOrigin;
    const float tca = FVector<float>::DotProduct(L, RayDirection);

    // Ray가 구를 등지고 있으면 miss
    // (tca < 0 이면 구 중심이 Ray 뒤쪽)
    if (tca < 0.0f)
    {
        // 단, Origin이 구 안에 있을 수도 있으므로 체크
        const float originInsideSq = FVector<float>::DotProduct(L, L);
        if (originInsideSq > Radius * Radius)
            return false;
    }
    const float Distance2 = FVector<float>::DotProduct(L, L) - (tca * tca);
    
    if (Distance2 > Radius * Radius)
        return false;
    
    return true;
}

bool UPrimitiveComponent::IntersectRayAABB(const FVector<float> &RayOrigin, const FVector<float> &RayDirection) 
{
    // ──────────────────────────────────────────────
    // 1. Local AABB를 World Space로 변환
    //    Rotation은 이 단계에서 무시 → AABB 특성상
    //    World Axis-Aligned로 재계산 (AABB의 한계)
    // ──────────────────────────────────────────────
    FTransform transform = GetTransformFromOwner();

    const FVector<float> LocalMin = GetLocalAABBMin();
    const FVector<float> LocalMax = GetLocalAABBMax();

    FVector<float> &WorldLocation = transform.Location;
    FVector<float> &WorldScale = transform.Scale;

    FVector<float> WorldMin = LocalMin * WorldScale + WorldLocation;
    FVector<float> WorldMax = LocalMax * WorldScale + WorldLocation;

    if (WorldMin.X > WorldMax.X)
        std::swap(WorldMin.X, WorldMax.X);
    if (WorldMin.Y > WorldMax.Y)
        std::swap(WorldMin.Y, WorldMax.Y);
    if (WorldMin.Z > WorldMax.Z)
        std::swap(WorldMin.Z, WorldMax.Z);

    // ──────────────────────────────────────────────
    // 2. Slab Method (Amy Williams, 2005)
    //    각 축별로 Ray가 Slab에 진입/탈출하는 t값 계산
    // ──────────────────────────────────────────────
    float tMin = 0.0f; // Ray 시작점 (음수 방향 교차 제거)
    float tMax = FLT_MAX;

    const float EPSILON = 1e-6f;
    // X Slab
    if (fabs(RayDirection.X) < EPSILON)
    {
        // Ray가 X축에 평행 → Origin이 Slab 밖이면 miss
        if (RayOrigin.X < WorldMin.X || RayOrigin.X > WorldMax.X)
            return false;
    }
    else
    {
        const float invDx = 1.0f / RayDirection.X;
        float       t1 = (WorldMin.X - RayOrigin.X) * invDx;
        float       t2 = (WorldMax.X - RayOrigin.X) * invDx; // P = Origin + t * Direction
        if (t1 > t2)
            std::swap(t1, t2);

        tMin = tMin > t1 ? tMin : t1;
        tMax = tMax > t2 ? t2 : tMax;
        if (tMin > tMax)
            return false;
    }
    // Y Slab
    if (fabs(RayDirection.Y) < EPSILON)
    {
        if (RayOrigin.Y < WorldMin.Y || RayOrigin.Y > WorldMax.Y)
            return false;
    }
    else
    {
        const float invDy = 1.0f / RayDirection.Y;
        float       t1 = (WorldMin.Y - RayOrigin.Y) * invDy;
        float       t2 = (WorldMax.Y - RayOrigin.Y) * invDy; // P = Origin + t * Direction
        if (t1 > t2)
            std::swap(t1, t2);

        tMin = tMin > t1 ? tMin : t1;
        tMax = tMax > t2 ? t2 : tMax;
        if (tMin > tMax)
            return false;
    }
    // Z Slab
    if (fabs(RayDirection.Z) < EPSILON)
    {
        if (RayOrigin.Z < WorldMin.Z || RayOrigin.Z > WorldMax.Z)
            return false;
    }
    else
    {
        const float invDz = 1.0f / RayDirection.Z;
        float       t1 = (WorldMin.Z - RayOrigin.Z) * invDz;
        float       t2 = (WorldMax.Z - RayOrigin.Z) * invDz; // P = Origin + t * Direction
        if (t1 > t2)
            std::swap(t1, t2);

        tMin = tMin > t1 ? tMin : t1;
        tMax = tMax > t2 ? t2 : tMax;
        if (tMin > tMax)
            return false;
    }

    return tMax >= 0.0f;
}

FHitResult UPrimitiveComponent::IntersectRayMeshTriangle(const FVector<float> &RayOrigin, const FVector<float> &RayDirection) 
{
    FHitResult Result;

    if (PrimitiveType == EPrimitiveType::None)
        return Result;

    TArray<FVertex> *vertices = UMeshManager::Get().GetVertexData(PrimitiveType);
    uint32           NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);

    TArray<uint16> *indices = UMeshManager::Get().GetIndexData(PrimitiveType);
    uint32            NumIndices = UMeshManager::Get().GetNumIndices(PrimitiveType);

    // World Matrix로 Vertex를 World Space로 변환
    FMatrix<float> WorldMatrix = GetWorldMatrix(); // TRS 행렬

    if (indices && NumIndices > 0)
    {
        // Index buffer 있는 경우
        for (uint32 i = 0; i + 2 < NumIndices; i += 3)
        {
            // index로 vertex 참조
            const FVertex &v0 = vertices->at(indices->at(i));
            const FVertex &v1 = vertices->at(indices->at(i + 1));
            const FVertex &v2 = vertices->at(indices->at(i + 2));

            FVector4<float> V0_L = {v0.Position.X, v0.Position.Y, v0.Position.Z, 1.f};
            FVector4<float> V1_L = {v1.Position.X, v1.Position.Y, v1.Position.Z, 1.f};
            FVector4<float> V2_L = {v2.Position.X, v2.Position.Y, v2.Position.Z, 1.f};

            FVector4<float> V0_W = V0_L * WorldMatrix;
            FVector4<float> V1_W = V1_L * WorldMatrix;
            FVector4<float> V2_W = V2_L * WorldMatrix;

            FVector<float> V0 = {V0_W.X, V0_W.Y, V0_W.Z};
            FVector<float> V1 = {V1_W.X, V1_W.Y, V1_W.Z};
            FVector<float> V2 = {V2_W.X, V2_W.Y, V2_W.Z};

            float T = 0.f;
            if (RayIntersectsTriangle(RayOrigin, RayDirection, V0, V1, V2, T))
            {
                if (T < Result.Distance)
                {
                    Result.bHit = true;
                    Result.Distance = T;
                    Result.HitPoint = RayOrigin + RayDirection * T;
                    Result.HitComponent = this;
                }
            }
        }
    }
    else
    {
        // Vertex 순회
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
    }

    return Result;
}
