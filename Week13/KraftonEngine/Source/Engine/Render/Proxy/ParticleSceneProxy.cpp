#include "Render/Proxy/ParticleSceneProxy.h"
#include "Component/ParticleSystemComponent.h"
#include "Particles/Rendering/ParticleRenderData.h"
#include "Particles/Common/ParticleTypes.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Materials/Material.h"
#include "Object/ObjectFactory.h"
#include "Math/Matrix.h"

// ============================================================
// FParticleSceneProxy — 생성 / 소멸
// ============================================================

FParticleSceneProxy::FParticleSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(static_cast<UPrimitiveComponent*>(InComponent))
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;
	ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;

	// 파티클 위치는 world-space 직접 전달 → Model = Identity
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	MarkPerObjectCBDirty();
}

FParticleSceneProxy::~FParticleSceneProxy()
{
	ReleaseParticleMaterials();
	DynVB.Release();
	DynIB.Release();
}

// ============================================================
// 내부 헬퍼
// ============================================================

FShader* FParticleSceneProxy::SelectParticleShader(EDynamicEmitterType Type)
{
	auto& SM = FShaderManager::Get();
	switch (Type)
	{
	case EDynamicEmitterType::DET_Sprite:  return SM.GetOrCreate(EShaderPath::ParticleSprite);
	case EDynamicEmitterType::DET_Mesh:    return SM.GetOrCreate(EShaderPath::ParticleMesh);
	case EDynamicEmitterType::DET_Beam:    return SM.GetOrCreate(EShaderPath::ParticleBeam);
	case EDynamicEmitterType::DET_Ribbon:  return SM.GetOrCreate(EShaderPath::ParticleRibbon);
	default:                               return nullptr;
	}
}

UMaterial* FParticleSceneProxy::CreateParticleMaterial(FShader* Shader)
{
	return UMaterial::CreateTransient(
		ERenderPass::AlphaBlend,
		EBlendState::AlphaBlend,
		EDepthStencilState::Default,
		ERasterizerState::SolidNoCull, // 파티클은 양면
		Shader);
}

void FParticleSceneProxy::ReleaseParticleMaterials()
{
	for (UMaterial* Mat : ParticleMaterials)
	{
		if (Mat) GUObjectArray.DestroyObject(Mat);
	}
	ParticleMaterials.clear();
}

// ============================================================
// UpdateMesh — EmitterRenderData를 캐싱 + proxy 소유 material 갱신
// (함수명은 FPrimitiveSceneProxy 인터페이스 레거시; Mesh 전용이 아님)
// ============================================================
void FParticleSceneProxy::UpdateMesh()
{
	UParticleSystemComponent* Comp = GetParticleComponent();
	CachedEmitters.clear();
	SectionDraws.clear();

	if (!Comp->IsParticleVisible() || !Comp->IsActive())
	{
		bVisible = false;
		return;
	}
	bVisible = true;

	const TArray<FDynamicEmitterDataBase*>& RenderData = Comp->GetEmitterRenderData();

	// emitter 수가 바뀌면 material 캐시 전체 재생성
	if (ParticleMaterials.size() != RenderData.size())
	{
		ReleaseParticleMaterials();
		ParticleMaterials.resize(RenderData.size(), nullptr);
	}

	CachedEmitters.reserve(RenderData.size());

	for (int32 i = 0; i < static_cast<int32>(RenderData.size()); ++i)
	{
		const FDynamicEmitterDataBase* Data = RenderData[i];
		if (!Data) continue;

		const FDynamicEmitterReplayDataBase& Source = Data->GetSource();
		if (!Source.IsValid()) continue;

		// emitter 타입 → shader permutation 선택
		FShader* Shader = SelectParticleShader(Source.eEmitterType);
		if (!Shader) continue; // Mesh 등 미지원 타입은 건너뜀

		// proxy 소유 material 생성 (최초 1회)
		if (!ParticleMaterials[i])
			ParticleMaterials[i] = CreateParticleMaterial(Shader);

		// 텍스처 SRV를 source material에서 매 틱 복사
		if (Source.Material)
		{
			const ID3D11ShaderResourceView* const* SrcSRVs = Source.Material->GetCachedSRVs();
			ParticleMaterials[i]->SetCachedSRV(
				EMaterialTextureSlot::Diffuse,
				const_cast<ID3D11ShaderResourceView*>(SrcSRVs[(int)EMaterialTextureSlot::Diffuse]));
		}

		CachedEmitters.push_back({ Data });
		// SectionDraws 범위는 UpdatePerViewport에서 vertex 조립 후 확정
		// 여기서는 proxy material만 SectionDraws와 순서를 맞추기 위해 보관
	}
}

// ============================================================
// UpdatePerViewport — GatherRenderData 위임 + SectionDraws 확정
// ============================================================
void FParticleSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	StagedInstances.clear();
	StagedIndices.clear();
	SectionDraws.clear();

	if (!bVisible || CachedEmitters.empty()) return;

	const FParticleVertexBuildContext Ctx
	{
		Frame.CameraRight,
		Frame.CameraUp,
		Frame.CameraForward
	};

	for (int32 i = 0; i < static_cast<int32>(CachedEmitters.size()); ++i)
	{
		const FCachedEmitter& E = CachedEmitters[i];
		if (!E.Data || i >= static_cast<int32>(ParticleMaterials.size()) || !ParticleMaterials[i]) continue;

		const uint32 FirstIndex = static_cast<uint32>(StagedIndices.size());

		// 타입별 구현에 위임 — 프록시는 타입을 알 필요 없음
		E.Data->GatherRenderData(Ctx, StagedInstances, StagedIndices);

		const uint32 IndexCount = static_cast<uint32>(StagedIndices.size()) - FirstIndex;
		if (IndexCount > 0)
			SectionDraws.push_back({ ParticleMaterials[i], FirstIndex, IndexCount });
	}
}

// ============================================================
// PrepareDrawBuffer — staging 버퍼를 GPU 동적 VB/IB에 업로드
// ============================================================
bool FParticleSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
	FDrawCommandBuffer& OutBuffer) const
{
	if (StagedInstances.empty())
		return false;

	const uint32 VCount = static_cast<uint32>(StagedInstances.size());
	const uint32 ICount  = static_cast<uint32>(StagedIndices.size());

	if (DynVB.GetStride() == 0)
		DynVB.Create(Device, VCount, sizeof(FSpriteParticleInstanceVertex));
	else
		DynVB.EnsureCapacity(Device, VCount);

	DynIB.EnsureCapacity(Device, ICount);

	if (!DynVB.Update(Context, StagedInstances.data(), VCount)) return false;
	if (!DynIB.Update(Context, StagedIndices.data(),  ICount)) return false;

	OutBuffer.VB       = DynVB.GetBuffer();
	OutBuffer.VBStride = DynVB.GetStride();
	OutBuffer.IB       = DynIB.GetBuffer();
	OutBuffer.IndexCount = ICount;
	OutBuffer.FirstIndex = 0;
	OutBuffer.BaseVertex = 0;

	return true;
}

UParticleSystemComponent* FParticleSceneProxy::GetParticleComponent() const
{
	return static_cast<UParticleSystemComponent*>(GetOwner());
}
