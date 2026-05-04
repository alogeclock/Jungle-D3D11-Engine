#include "ItemActorBase.h"

#include "Component/BillboardComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(AItemActorBase, AActor)

AItemActorBase::AItemActorBase()
{
	// BoxComponent
	ItemTrigger = AddComponent<UBoxComponent>();
	ItemTrigger->SetCanDeleteFromDetails(false);
	ItemTrigger->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	ItemTrigger->SetCollisionEnabled(InteractionConfig.bStartsEnabled);
	ItemTrigger->SetGenerateOverlapEvents(true);
	SetRootComponent(ItemTrigger);

	// Billboard
	ItemImage = AddComponent<UBillboardComponent>();
	ItemImage->SetCollisionEnabled(false);
	ItemImage->SetGenerateOverlapEvents(false);
	ItemImage->AttachToComponent(GetRootComponent());

	// 기본 script 부착
	// item별 동작은 SetItemScript("Scripts/Game/Items/LogItem.lua")로 확장
	ItemScript = AddComponent<UScriptComponent>();
	ItemScript->SetScriptPath("Scripts/Game/Items/ItemBase.lua");
}

void AItemActorBase::BeginPlay()
{
	// BeginPlay 직전에 다시 보정.
	// Details 창 수정, deserialize, spawn 이후 값 변경이 섞여도 최종 상태를 맞춘다.
	if (ItemTrigger)
	{
		ItemTrigger->SetCollisionEnabled(InteractionConfig.bStartsEnabled);
		ItemTrigger->SetGenerateOverlapEvents(InteractionConfig.bStartsEnabled);
	}

	if (ItemImage)
	{
		ItemImage->SetCollisionEnabled(false);
		ItemImage->SetGenerateOverlapEvents(false);

		if (GetRootComponent())
		{
			ItemImage->AttachToComponent(GetRootComponent());
		}
	}

	// Texture는 반드시 BeginPlay에서 적용.
	// 생성자에서 넣으면 renderer/resource 준비 시점 문제로 적용이 안 되는 경우가 있었음.
	if (ItemImage && !ItemTexturePath.empty() && ItemTexturePath != "None" && GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (Device)
		{
			if (UTexture2D* Texture = UTexture2D::LoadFromFile(ItemTexturePath, Device))
			{
				ItemImage->SetTexture(Texture);
			}
		}
	}

	// 중요:
	// ScriptComponent의 BeginPlay/Lua BeginPlay가 Super::BeginPlay() 내부에서 돈다면,
	// texture 적용이 Lua BeginPlay보다 먼저 끝난다.
	Super::BeginPlay();
}

void AItemActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AItemActorBase::EndPlay()
{
	Super::EndPlay();
}

void AItemActorBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// C++ config는 future-proof 저장 데이터입니다.
	// 현재 gameplay 튜닝은 Lua property가 담당하지만, scene 저장/로드 시 기본값이 사라지지 않게 보존합니다.
	Ar << ItemFeatureFlags;
	Ar << InteractionConfig.ScoreValue;
	Ar << InteractionConfig.RequiredInteractorTag;
	Ar << InteractionConfig.EffectName;
	Ar << InteractionConfig.EffectDuration;
	Ar << InteractionConfig.RespawnDelay;
	Ar << InteractionConfig.Cooldown;
	Ar << InteractionConfig.bStartsEnabled;
	Ar << InteractionConfig.bDestroyOnPickup;

	// BeginPlay에서 texture를 다시 적용할 수 있게 경로도 저장.
	Ar << ItemTexturePath;
}

UPrimitiveComponent* AItemActorBase::GetItemTrigger() const
{
	return ItemTrigger;
}

void AItemActorBase::SetItemScript(const FString& ScriptPath)
{
	if (!ItemScript)
	{
		ItemScript = AddComponent<UScriptComponent>();
	}

	if (ItemScript)
	{
		ItemScript->SetScriptPath(ScriptPath);
	}
}

void AItemActorBase::SetItemTexturePath(const FString& TexturePath)
{
	if (TexturePath.empty())
	{
		ItemTexturePath = "None";
		return;
	}

	ItemTexturePath = TexturePath;
}

bool AItemActorBase::HasFeature(EItemFeatureFlags Feature) const
{
	return (ItemFeatureFlags & static_cast<uint32>(Feature)) != 0;
}

void AItemActorBase::SetFeatureEnabled(EItemFeatureFlags Feature, bool bEnabled)
{
	if (bEnabled)
	{
		AddFeature(Feature);
	}
	else
	{
		RemoveFeature(Feature);
	}
}

void AItemActorBase::AddFeature(EItemFeatureFlags Feature)
{
	ItemFeatureFlags |= static_cast<uint32>(Feature);
}

void AItemActorBase::RemoveFeature(EItemFeatureFlags Feature)
{
	ItemFeatureFlags &= ~static_cast<uint32>(Feature);
}

void AItemActorBase::SetTriggerEnabled(bool bEnabled)
{
	InteractionConfig.bStartsEnabled = bEnabled;

	if (ItemTrigger)
	{
		ItemTrigger->SetCollisionEnabled(bEnabled);
		ItemTrigger->SetGenerateOverlapEvents(bEnabled);
	}
}

bool AItemActorBase::IsTriggerEnabled() const
{
	return ItemTrigger && ItemTrigger->IsCollisionEnabled();
}
