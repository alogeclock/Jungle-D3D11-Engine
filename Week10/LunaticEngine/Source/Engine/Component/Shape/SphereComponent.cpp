#include "SphereComponent.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cmath>

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

void USphereComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Sphere Radius", EPropertyType::Float, &SphereRadius, 0.0f, 2048.f, 0.1f });
}

void USphereComponent::Serialize(FArchive& Ar) {
	UShapeComponent::Serialize(Ar);
	Ar << SphereRadius;
}

void USphereComponent::DrawDebugShape(FScene& Scene) const {
	if (SphereRadius <= 0.f) return;
	constexpr uint32 Segments = 24;
	
	FVector Center = GetWorldLocation();

	DrawDebugRing(Center, SphereRadius, FVector(1, 0, 0), FVector(0, 1, 0), Segments, false, Scene);
	DrawDebugRing(Center, SphereRadius, FVector(1, 0, 0), FVector(0, 0, 1), Segments, false, Scene);
	DrawDebugRing(Center, SphereRadius, FVector(0, 1, 0), FVector(0, 0, 1), Segments, false, Scene);
}

void USphereComponent::UpdateWorldAABB() const {
	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(SphereRadius, SphereRadius, SphereRadius);
	WorldAABBMaxLocation = WorldCenter + FVector(SphereRadius, SphereRadius, SphereRadius);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

bool USphereComponent::LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult)
{
	const FVector Center = GetWorldLocation();
	const FVector ToRay = Ray.Origin - Center;
	const float A = Ray.Direction.Dot(Ray.Direction);
	const float B = 2.0f * ToRay.Dot(Ray.Direction);
	const float C = ToRay.Dot(ToRay) - SphereRadius * SphereRadius;
	const float Discriminant = B * B - 4.0f * A * C;
	if (Discriminant < 0.0f || std::abs(A) < 1e-8f)
	{
		return false;
	}

	const float SqrtDiscriminant = std::sqrt(Discriminant);
	const float InvDenom = 1.0f / (2.0f * A);
	float T = (-B - SqrtDiscriminant) * InvDenom;
	if (T < 0.0f)
	{
		T = (-B + SqrtDiscriminant) * InvDenom;
	}
	if (T < 0.0f)
	{
		return false;
	}

	OutHitResult.HitComponent = this;
	OutHitResult.Distance = T;
	OutHitResult.WorldHitLocation = Ray.Origin + Ray.Direction * T;
	OutHitResult.WorldNormal = (OutHitResult.WorldHitLocation - Center).Normalized();
	OutHitResult.bHit = true;
	return true;
}
