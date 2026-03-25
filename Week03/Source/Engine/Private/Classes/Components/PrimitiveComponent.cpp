#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include "Source/Editor/Public/Application.h"
#include "World.h"

UPrimitiveComponent::UPrimitiveComponent(const FString &InString) : USceneComponent(InString) 
{
    // 1. 다형성을 이용해 자식 클래스에 맞는 Proxy 메모리 동적 할당
    if (!RenderProxy)
    {
        RenderProxy = CreateRenderProxy();
        
        // 2. FScene에 Proxy의 포인터를 등록 (렌더러가 순회할 수 있도록)
        if (RenderProxy && GMainScene)
        {
            GMainScene->AddProxy(RenderProxy);
        }
    }
}

UPrimitiveComponent::~UPrimitiveComponent() 
{
    if (RenderProxy)
    {
        if (GMainScene)
        {
            GMainScene->RemoveProxy(RenderProxy);
        }
        delete RenderProxy;
        RenderProxy = nullptr;
    }
}

void UPrimitiveComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    FRenderCommand &Command = RenderProxy->RenderCommand;

    Command.bIsVisible = this->bIsVisible; 

    if (!bIsVisible)
        return;

    Command.Constants.MVPMatrix = GetWorldMatrix();    
    Command.ConstantsColor = FConstantsColor(Color.X, Color.Y, Color.Z, Color.W);
    Command.bEnableDepthTest = bEnableDepthTest;
    Command.bIgnoreWireframe = bIsInEditor;
    Command.CullMode = CullMode;
    Command.Topology = this->Topology;

    bool bGlobalDrawAABB = (ViewOptions.ShowFlags & EEngineShowFlags::SF_AABB) != EEngineShowFlags::None;
    if (bShowAABB && bGlobalDrawAABB && GWorld && GWorld->GetLineBatcherComponent())
    {
        UpdateBounds(); 
        GWorld->GetLineBatcherComponent()->DrawBox(WorldAABB, {0.3f, 1.0f, 0.3f, 1.0f});
    }
}

FRenderProxy* UPrimitiveComponent::CreateRenderProxy() 
{
    RenderProxy = new FRenderProxy();
    return RenderProxy;
}

// 오직 Editor용으로 사용되는 함수 (일반적인 컴포넌트는 쓸 수 없음!)
// Texture가 있는 에디터 컴포넌트일 경우 오버라이딩해서 처리해야 함! (BillboardComponent 참조)
void UPrimitiveComponent::Render(URenderer &renderer)
{
    if (!IsRenderable(renderer))
        return;

    FConstants constants;
    constants.MVPMatrix = GetWorldMatrix(); // 부모 및 자신의 변경 사항을 반영한 GetWorldMatrix()를 호출한다.

    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);
    renderer.SetTopology(this->Topology);

    FConstantsColor constantsColor(Color.X, Color.Y, Color.Z, Color.W);
    
    renderer.DeviceContext->VSSetShader(renderer.SimpleVertexShader, nullptr, 0);
    renderer.DeviceContext->PSSetShader(renderer.SimplePixelShader, nullptr, 0);
    renderer.DeviceContext->IASetInputLayout(renderer.SimpleInputLayout);

    renderer.RenderPrimitive(this, constants, constantsColor);
}

void UPrimitiveComponent::SetPrimitiveType(EPrimitiveType InType)
{
    if (PrimitiveType != InType)
    {
        PrimitiveType = InType;
        bLocalBoundsDirty = true;
    }
}

// 선택되었을 때의 화면에 어떻게 그려질지 효과를 정의하는 함수
void UPrimitiveComponent::SetSelectEffect(bool Selected)
{
    if (Selected)
    {
        SetColor({0.0f, 0.0f, 0.0f, 0.5f});
        bShowAABB = true;
        bShowUUID = true;
    }
    else
    {
        SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        bShowAABB = false;
        bShowUUID = false;
    }
}

// 에디터 컴포넌트용 체크 함수
bool UPrimitiveComponent::IsRenderable(URenderer &renderer)
{
    if (!bIsVisible)
        return false;

    // Show Primitives 설정이 꺼져 있고, 에디터 컴포넌트가 아니라면 렌더링을 취소한다.
    if (!bIsInEditor && !renderer.CheckShowFlag(EEngineShowFlags::SF_Primitives))
        return false;

    return true;
}

FHitResult UPrimitiveComponent::IntersectRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection)
{
    FHitResult Result;

    if (!bIsVisible || PrimitiveType == EPrimitiveType::None)
        return Result;

    switch (PrimitiveType)
    {
    // Vertex 적음 → 바로 Triangle 검사
    case EPrimitiveType::Triangle:
    case EPrimitiveType::Plane:
    case EPrimitiveType::Text:
    case EPrimitiveType::SubUV:
        Result = IntersectRayMeshTriangle(RayOrigin, RayDirection);
        break;

    // Vertex 많음 → Sphere -> AABB → Triangle
    case EPrimitiveType::Cube:
    case EPrimitiveType::Sphere:
    case EPrimitiveType::Arrow:
    case EPrimitiveType::CubeArrow:
    case EPrimitiveType::Ring:
        if (!IntersectRayBoundingSphere(RayOrigin, RayDirection))
            return Result;
        if (!IntersectRayAABB(RayOrigin, RayDirection))
            return Result;
        Result = IntersectRayMeshTriangle(RayOrigin, RayDirection);
        break;
    default: 
        return Result;
    }

    return Result;
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
    FTransform transform = transform.ToTransform(GetWorldMatrix());
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
    const float          tca = FVector<float>::DotProduct(L, RayDirection);

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
    UpdateBounds();

    FVector<float> WorldMin = WorldAABB.Min;
    FVector<float> WorldMax = WorldAABB.Max;

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

    if (PrimitiveType == EPrimitiveType::None || PrimitiveType == EPrimitiveType::Text)
        return Result;

    uint32 NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);
    TArray<uint16>* Indices = UMeshManager::Get().GetIndexData(PrimitiveType);
    uint32 NumIndices = UMeshManager::Get().GetNumIndices(PrimitiveType);
    FMatrix<float> WorldMatrix = GetWorldMatrix();

    // 1. 텍스처 여부에 따라 필요한 정점 배열 포인터만 미리 가져옵니다.
    // (이 컴포넌트가 텍스처를 쓰는지 판별하는 로직은 상황에 맞게 넣어주세요)
    TArray<FVertex>* Vertices = nullptr;
    TArray<FTextureVertex>* TextureVertices = nullptr;

    if (bIsTextured)
        TextureVertices = UMeshManager::Get().GetVertexData<FTextureVertex>(PrimitiveType);
    else
        Vertices = UMeshManager::Get().GetVertexData<FVertex>(PrimitiveType);

    // 2. 핵심: 인덱스 번호를 넣으면 알맞은 배열에서 Position만 꺼내주는 람다 함수
    auto GetPosition = [&](uint32 Index) -> FVector<float>& {
        if (bIsTextured && TextureVertices)
            return (*TextureVertices)[Index].Position;
        else if (Vertices)
            return (*Vertices)[Index].Position;
    };

    // 1. 루프 진입 전, WorldMatrix의 역행렬을 계산 (단 1번만 수행)
    FMatrix<float> InverseWorldMatrix = GetWorldMatrix().Inverse();

    // 2. Ray의 Origin을 Local Space로 변환 (점 위치이므로 w = 1.0f 적용)
    FVector4<float> LocalOrigin4 = FVector4<float>(RayOrigin.X, RayOrigin.Y, RayOrigin.Z, 1.0f) * InverseWorldMatrix;
    FVector<float> LocalOrigin = {LocalOrigin4.X, LocalOrigin4.Y, LocalOrigin4.Z};

    // 3. Ray의 Direction을 Local Space로 변환 (방향 벡터이므로 이동(Translation)을 무시하기 위해 w = 0.0f 적용)
    FVector4<float> LocalDir4 = FVector4<float>(RayDirection.X, RayDirection.Y, RayDirection.Z, 0.0f) * InverseWorldMatrix;
    FVector<float> LocalDir = {LocalDir4.X, LocalDir4.Y, LocalDir4.Z};

    if (Indices && NumIndices > 0)
    {
        // Index buffer 있는 경우
        for (uint32 i = 0; i + 2 < NumIndices; i += 3)
        {
            // index로 vertex 참조
            const FVector<float>& V0 = GetPosition((*Indices)[i]);
            const FVector<float>& V1 = GetPosition((*Indices)[i + 1]);
            const FVector<float>& V2 = GetPosition((*Indices)[i + 2]);

            float T = 0.f;
            if (RayIntersectsTriangle(LocalOrigin, LocalDir, V0, V1, V2, T))
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
            FVector<float> V0 = GetPosition(i);
            FVector<float> V1 = GetPosition(i + 1);
            FVector<float> V2 = GetPosition(i + 2);

            float T = 0.f;
            if (RayIntersectsTriangle(LocalOrigin, LocalDir, V0, V1, V2, T))
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

void UPrimitiveComponent::UpdateBounds()
{
    if (bLocalBoundsDirty)
    {
        LocalAABB = UMeshManager::Get().GetMeshAABB(PrimitiveType);

        FVector<float> Min = LocalAABB.Min;
        FVector<float> Max = LocalAABB.Max;                                                               

        bLocalBoundsDirty = false;
    }

    FMatrix<float> WorldMatrix = GetWorldMatrix();

    // 1. 로컬 AABB의 중심(Center)과 반직경(Extents) 계산
    FVector<float> LocalCenter = (LocalAABB.Max + LocalAABB.Min) * 0.5f;
    FVector<float> LocalExtents = (LocalAABB.Max - LocalAABB.Min) * 0.5f;

    // 2. 중심점 이동 (Center * Matrix)
    FVector4<float> WorldCenter4 = FVector4<float>(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, 1.0f) * WorldMatrix;
    FVector<float>  WorldCenter = FVector<float>(WorldCenter4.X, WorldCenter4.Y, WorldCenter4.Z);

    // 3. 회전 행렬의 절댓값을 적용하여 새로운 Extents 계산 (Scale 적용됨)
    // (WorldMatrix의 3x3 부분의 절댓값과 LocalExtents를 내적)
    FVector<float> WorldExtents(
        std::abs(WorldMatrix.M[0][0]) * LocalExtents.X + std::abs(WorldMatrix.M[1][0]) * LocalExtents.Y + std::abs(WorldMatrix.M[2][0]) * LocalExtents.Z,
        std::abs(WorldMatrix.M[0][1]) * LocalExtents.X + std::abs(WorldMatrix.M[1][1]) * LocalExtents.Y + std::abs(WorldMatrix.M[2][1]) * LocalExtents.Z,
        std::abs(WorldMatrix.M[0][2]) * LocalExtents.X + std::abs(WorldMatrix.M[1][2]) * LocalExtents.Y + std::abs(WorldMatrix.M[2][2]) * LocalExtents.Z);

    // 4. 새로운 World AABB 갱신
    WorldAABB.Min = WorldCenter - WorldExtents;
    WorldAABB.Max = WorldCenter + WorldExtents;
}

void UPrimitiveComponent::UpdateBoundsTexture(TArray<FTextureVertex>* vertices)
{
    if (bLocalBoundsDirty)
    {
        LocalAABB = UMeshManager::Get().ComputeAABB(*vertices);

        FVector<float> Min = LocalAABB.Min;
        FVector<float> Max = LocalAABB.Max;                                                               

        bLocalBoundsDirty = false;
    }

    FMatrix<float> WorldMatrix = GetWorldMatrix();

    // 1. 로컬 AABB의 중심(Center)과 반직경(Extents) 계산
    FVector<float> LocalCenter = (LocalAABB.Max + LocalAABB.Min) * 0.5f;
    FVector<float> LocalExtents = (LocalAABB.Max - LocalAABB.Min) * 0.5f;

    // 2. 중심점 이동 (Center * Matrix)
    FVector4<float> WorldCenter4 = FVector4<float>(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, 1.0f) * WorldMatrix;
    FVector<float>  WorldCenter = FVector<float>(WorldCenter4.X, WorldCenter4.Y, WorldCenter4.Z);

    // 3. 회전 행렬의 절댓값을 적용하여 새로운 Extents 계산 (Scale 적용됨)
    // (WorldMatrix의 3x3 부분의 절댓값과 LocalExtents를 내적)
    FVector<float> WorldExtents(
        std::abs(WorldMatrix.M[0][0]) * LocalExtents.X + std::abs(WorldMatrix.M[1][0]) * LocalExtents.Y + std::abs(WorldMatrix.M[2][0]) * LocalExtents.Z,
        std::abs(WorldMatrix.M[0][1]) * LocalExtents.X + std::abs(WorldMatrix.M[1][1]) * LocalExtents.Y + std::abs(WorldMatrix.M[2][1]) * LocalExtents.Z,
        std::abs(WorldMatrix.M[0][2]) * LocalExtents.X + std::abs(WorldMatrix.M[1][2]) * LocalExtents.Y + std::abs(WorldMatrix.M[2][2]) * LocalExtents.Z);

    // 4. 새로운 World AABB 갱신
    WorldAABB.Min = WorldCenter - WorldExtents;
    WorldAABB.Max = WorldCenter + WorldExtents;
}

const FBox &UPrimitiveComponent::GetWorldAABB()
{
    UpdateBounds();
    return WorldAABB;
}
