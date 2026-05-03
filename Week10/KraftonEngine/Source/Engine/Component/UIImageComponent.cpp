#include "Component/UIImageComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

#include <algorithm>

IMPLEMENT_CLASS(UUIImageComponent, UBillboardComponent)

namespace
{
	constexpr const char* DefaultUIImageTexture = "Asset/Content/Texture/checker.png";
}

UUIImageComponent::UUIImageComponent()
{
	bTickEnable = false;
	SetTexturePath(DefaultUIImageTexture);
	SetCanDeleteFromDetails(true);
}

void UUIImageComponent::Serialize(FArchive& Ar)
{
	UBillboardComponent::Serialize(Ar);
	Ar << ScreenPosition;
	Ar << ScreenSize;
	Ar << Tint;
	Ar << ZOrder;
}

void UUIImageComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::TextureSlot, &TextureSlot });
	OutProps.push_back({ "Screen Position", EPropertyType::Vec3, &ScreenPosition, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Screen Size", EPropertyType::Vec3, &ScreenSize, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Tint", EPropertyType::Color4, &Tint });
	OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
}

void UUIImageComponent::PostEditProperty(const char* PropertyName)
{
	UBillboardComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0 && (TextureSlot.Path.empty() || TextureSlot.Path == "None"))
	{
		SetTexturePath(DefaultUIImageTexture);
	}
	else if (strcmp(PropertyName, "Screen Size") == 0)
	{
		SetScreenSize(ScreenSize);
	}
}

void UUIImageComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	if (ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV())
	{
		Scene.AddScreenQuad(
			SRV,
			FVector2(ScreenPosition.X, ScreenPosition.Y),
			FVector2(ScreenSize.X, ScreenSize.Y),
			Tint,
			ZOrder);
	}
}

void UUIImageComponent::SetScreenSize(const FVector& InScreenSize)
{
	ScreenSize.X = (std::max)(1.0f, InScreenSize.X);
	ScreenSize.Y = (std::max)(1.0f, InScreenSize.Y);
	ScreenSize.Z = InScreenSize.Z;

	Width = ScreenSize.X;
	Height = ScreenSize.Y;
}

bool UUIImageComponent::SetTexturePath(const FString& InTexturePath)
{
	TextureSlot.Path = InTexturePath.empty() ? FString(DefaultUIImageTexture) : InTexturePath;
	return EnsureTextureLoaded();
}

bool UUIImageComponent::EnsureTextureLoaded()
{
	if (TextureSlot.Path.empty() || TextureSlot.Path == "None")
	{
		TextureSlot.Path = DefaultUIImageTexture;
	}

	if (ResolveTextureFromPath(TextureSlot.Path))
	{
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	if (UTexture2D* LoadedTexture = UTexture2D::LoadFromFile(TextureSlot.Path, Device))
	{
		SetTexture(LoadedTexture);
		return true;
	}

	return false;
}

ID3D11ShaderResourceView* UUIImageComponent::GetResolvedTextureSRV() const
{
	if (Texture && Texture->IsLoaded())
	{
		return Texture->GetSRV();
	}

	return nullptr;
}
