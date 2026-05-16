#include "HeightFogComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UHeightFogComponent, USceneComponent)

BEGIN_CLASS_PROPERTIES(UHeightFogComponent)
	PROPERTY_FLOAT(FogDensity, "Fog Density", "Fog", 0.0f, 0.05f, 0.001f, CPF_Edit)
	PROPERTY_FLOAT(FogHeightFalloff, "Height Falloff", "Fog", 0.001f, 5.0f, 0.01f, CPF_Edit)
	PROPERTY_FLOAT(StartDistance, "Start Distance", "Fog", 0.0f, 100000.0f, 1.0f, CPF_Edit)
	PROPERTY_FLOAT(FogCutoffDistance, "Cutoff Distance", "Fog", 0.0f, 100000.0f, 1.0f, CPF_Edit)
	PROPERTY_FLOAT(FogMaxOpacity, "Max Opacity", "Fog", 0.0f, 1.0f, 0.01f, CPF_Edit)
	REGISTER_PROPERTY(FogInscatteringColor, "Inscattering Color", EPropertyType::Color4, "Fog", CPF_Edit)
END_CLASS_PROPERTIES(UHeightFogComponent)

UHeightFogComponent::UHeightFogComponent()
{
	SetComponentTickEnabled(false);
}

void UHeightFogComponent::CreateRenderState()
{
	PushToScene();
}

void UHeightFogComponent::DestroyRenderState()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().GetEnvironment().RemoveFog(this);
}

void UHeightFogComponent::OnTransformDirty()
{
	USceneComponent::OnTransformDirty();
	PushToScene();
}

void UHeightFogComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	FFogParams Params;
	Params.Density = FogDensity;
	Params.HeightFalloff = FogHeightFalloff;
	Params.StartDistance = StartDistance;
	Params.CutoffDistance = FogCutoffDistance;
	Params.MaxOpacity = FogMaxOpacity;
	Params.FogBaseHeight = GetWorldLocation().Z;
	Params.InscatteringColor = FogInscatteringColor;

	World->GetScene().GetEnvironment().AddFog(this, Params);
}

void UHeightFogComponent::PostEditProperty(const char* PropertyName)
{
	USceneComponent::PostEditProperty(PropertyName);
	PushToScene();
}

void UHeightFogComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);

	Ar << FogDensity;
	Ar << FogHeightFalloff;
	Ar << StartDistance;
	Ar << FogCutoffDistance;
	Ar << FogMaxOpacity;
	Ar << FogInscatteringColor;
}

UBillboardComponent* UHeightFogComponent::EnsureEditorBillboard()
{
	if (!Owner)
	{
		return nullptr;
	}

	for (USceneComponent* Child : GetChildren())
	{
		UBillboardComponent* Billboard = Cast<UBillboardComponent>(Child);
		if (Billboard && Billboard->IsEditorOnlyComponent())
		{
			// 에디터 아이콘 빌보드는 부모 스케일과 컴포넌트 트리 기본 표시에서 분리한다.
			Billboard->SetAbsoluteScale(true);
			Billboard->SetHiddenInComponentTree(true);
			return Billboard;
		}
	}

	UBillboardComponent* Billboard = Owner->AddComponent<UBillboardComponent>();
	if (Billboard)
	{
		Billboard->AttachToComponent(this);
		// 에디터 아이콘 빌보드는 부모 스케일과 컴포넌트 트리 기본 표시에서 분리한다.
		Billboard->SetAbsoluteScale(true);
		Billboard->SetEditorOnlyComponent(true);
		Billboard->SetHiddenInComponentTree(true);
		auto Material = FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/Editor/HeightFog.mat");
		Billboard->SetMaterial(Material);
	}

	return Billboard;
}
