#include "Component/UWindowPanelComponent.h"

#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

#include <algorithm>

IMPLEMENT_CLASS(UWindowPanelComponent, UUIImageComponent)

namespace
{
	void FitBordersToTarget(float TargetSize, float& StartSize, float& EndSize)
	{
		StartSize = (std::max)(0.0f, StartSize);
		EndSize = (std::max)(0.0f, EndSize);

		const float Total = StartSize + EndSize;
		if (Total > TargetSize && Total > 0.0f)
		{
			const float Scale = TargetSize / Total;
			StartSize *= Scale;
			EndSize *= Scale;
		}
	}
}

UWindowPanelComponent::UWindowPanelComponent()
{
	Slice = FVector4(8.0f, 24.0f, 8.0f, 8.0f);
}

void UWindowPanelComponent::Serialize(FArchive& Ar)
{
	UUIImageComponent::Serialize(Ar);
	Ar << Slice;
	Ar << bDrawBorder;
	Ar << bDrawCenter;
}

void UWindowPanelComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UUIImageComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Slice", EPropertyType::Vec4, &Slice, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Draw Border", EPropertyType::Bool, &bDrawBorder });
	OutProps.push_back({ "Draw Center", EPropertyType::Bool, &bDrawCenter });
}

void UWindowPanelComponent::PostEditProperty(const char* PropertyName)
{
	UUIImageComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Slice") == 0 || strcmp(PropertyName, "Texture") == 0)
	{
		ClampSlice();
	}
}

void UWindowPanelComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV();
	UTexture2D* Texture2D = GetTexture();
	if (!SRV || !Texture2D || !Texture2D->IsLoaded())
	{
		return;
	}

	const float TextureWidth = static_cast<float>(Texture2D->GetWidth());
	const float TextureHeight = static_cast<float>(Texture2D->GetHeight());
	if (TextureWidth <= 0.0f || TextureHeight <= 0.0f)
	{
		return;
	}

	float SliceLeft = (std::clamp)(Slice.X, 0.0f, TextureWidth);
	float SliceTop = (std::clamp)(Slice.Y, 0.0f, TextureHeight);
	float SliceRight = (std::clamp)(Slice.Z, 0.0f, TextureWidth - SliceLeft);
	float SliceBottom = (std::clamp)(Slice.W, 0.0f, TextureHeight - SliceTop);

	float LeftWidth = SliceLeft;
	float RightWidth = SliceRight;
	float TopHeight = SliceTop;
	float BottomHeight = SliceBottom;

	FitBordersToTarget(ScreenSize.X, LeftWidth, RightWidth);
	FitBordersToTarget(ScreenSize.Y, TopHeight, BottomHeight);

	const float CenterWidth = (std::max)(0.0f, ScreenSize.X - LeftWidth - RightWidth);
	const float CenterHeight = (std::max)(0.0f, ScreenSize.Y - TopHeight - BottomHeight);

	const float X0 = ScreenPosition.X;
	const float X1 = X0 + LeftWidth;
	const float X2 = X0 + ScreenSize.X - RightWidth;
	const float Y0 = ScreenPosition.Y;
	const float Y1 = Y0 + TopHeight;
	const float Y2 = Y0 + ScreenSize.Y - BottomHeight;

	const float U0 = 0.0f;
	const float U1 = SliceLeft / TextureWidth;
	const float U2 = 1.0f - (SliceRight / TextureWidth);
	const float U3 = 1.0f;
	const float V0 = 0.0f;
	const float V1 = SliceTop / TextureHeight;
	const float V2 = 1.0f - (SliceBottom / TextureHeight);
	const float V3 = 1.0f;

	if (bDrawBorder)
	{
		AddPanelQuad(Scene, SRV, X0, Y0, LeftWidth, TopHeight, U0, V0, U1, V1);
		AddPanelQuad(Scene, SRV, X1, Y0, CenterWidth, TopHeight, U1, V0, U2, V1);
		AddPanelQuad(Scene, SRV, X2, Y0, RightWidth, TopHeight, U2, V0, U3, V1);

		AddPanelQuad(Scene, SRV, X0, Y1, LeftWidth, CenterHeight, U0, V1, U1, V2);
		AddPanelQuad(Scene, SRV, X2, Y1, RightWidth, CenterHeight, U2, V1, U3, V2);

		AddPanelQuad(Scene, SRV, X0, Y2, LeftWidth, BottomHeight, U0, V2, U1, V3);
		AddPanelQuad(Scene, SRV, X1, Y2, CenterWidth, BottomHeight, U1, V2, U2, V3);
		AddPanelQuad(Scene, SRV, X2, Y2, RightWidth, BottomHeight, U2, V2, U3, V3);
	}

	if (bDrawCenter)
	{
		AddPanelQuad(Scene, SRV, X1, Y1, CenterWidth, CenterHeight, U1, V1, U2, V2);
	}
}

void UWindowPanelComponent::ClampSlice()
{
	Slice.X = (std::max)(0.0f, Slice.X);
	Slice.Y = (std::max)(0.0f, Slice.Y);
	Slice.Z = (std::max)(0.0f, Slice.Z);
	Slice.W = (std::max)(0.0f, Slice.W);

	UTexture2D* Texture2D = GetTexture();
	if (!Texture2D || !Texture2D->IsLoaded())
	{
		return;
	}

	const float TextureWidth = static_cast<float>(Texture2D->GetWidth());
	const float TextureHeight = static_cast<float>(Texture2D->GetHeight());
	Slice.X = (std::clamp)(Slice.X, 0.0f, TextureWidth);
	Slice.Y = (std::clamp)(Slice.Y, 0.0f, TextureHeight);
	Slice.Z = (std::clamp)(Slice.Z, 0.0f, TextureWidth - Slice.X);
	Slice.W = (std::clamp)(Slice.W, 0.0f, TextureHeight - Slice.Y);
}

void UWindowPanelComponent::AddPanelQuad(FScene& Scene, ID3D11ShaderResourceView* SRV, float X, float Y, float Width, float Height,
	float U0, float V0, float U1, float V1) const
{
	if (Width <= 0.0f || Height <= 0.0f)
	{
		return;
	}

	Scene.AddScreenQuad(
		SRV,
		FVector2(X, Y),
		FVector2(Width, Height),
		Tint,
		ZOrder,
		FVector2(U0, V0),
		FVector2(U1, V1));
}
