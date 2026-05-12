#include "CapsuleComponent.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cfloat>
#include <cmath>

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	UShapeComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Capsule Radius",		EPropertyType::Float, &CapsuleRadius,	  0.0f, 2048.f, 0.1f });
	OutProps.push_back({ "Capsule Half-Height", EPropertyType::Float, &CapsuleHalfHeight, 0.0f, 2048.f, 0.1f });
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName) {
	if (strcmp(PropertyName, "Capsule Radius") == 0)
		CapsuleRadius = CapsuleRadius < CapsuleHalfHeight ? CapsuleRadius : CapsuleHalfHeight;
	else if (strcmp(PropertyName, "Capsule Half-Height") == 0)
		CapsuleHalfHeight = CapsuleHalfHeight > CapsuleRadius ? CapsuleHalfHeight : CapsuleRadius;
	UShapeComponent::PostEditProperty(PropertyName);
}

void UCapsuleComponent::Serialize(FArchive& Ar) {
	UShapeComponent::Serialize(Ar);
	Ar << CapsuleRadius;
	Ar << CapsuleHalfHeight;
}

void UCapsuleComponent::DrawDebugShape(FScene& Scene) const {
	if (CapsuleRadius <= 0.f || CapsuleHalfHeight < 0.f) return;

	FVector Center    = GetWorldLocation();
	FVector Up        = GetUpVector().Normalized();
	FVector Fwd       = GetForwardVector().Normalized();
	FVector Right     = GetRightVector().Normalized();

	float BiasedHeight = CapsuleHalfHeight > CapsuleRadius ? (CapsuleHalfHeight - CapsuleRadius) : 0.f;
	FVector TopCenter = Center + Up * BiasedHeight;
	FVector BotCenter = Center - Up * BiasedHeight;
	FVector NegUp     = Up * -1.f;

	constexpr uint32 Segments     = 24;
	constexpr uint32 HalfSegments = Segments / 2;

	// Junction rings at top and bottom
	DrawDebugRing(TopCenter, CapsuleRadius, Fwd, Right, Segments,     false, Scene);
	DrawDebugRing(BotCenter, CapsuleRadius, Fwd, Right, Segments,     false, Scene);

	// Top hemisphere
	DrawDebugRing(TopCenter, CapsuleRadius, Fwd,  Up,    HalfSegments, true, Scene);
	DrawDebugRing(TopCenter, CapsuleRadius, Right, Up,    HalfSegments, true, Scene);

	// Bottom hemisphere
	DrawDebugRing(BotCenter, CapsuleRadius, Fwd,  NegUp, HalfSegments, true, Scene);
	DrawDebugRing(BotCenter, CapsuleRadius, Right, NegUp, HalfSegments, true, Scene);

	// Body lines
	Scene.AddDebugLine(TopCenter + Fwd   * CapsuleRadius, BotCenter + Fwd   * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter - Fwd   * CapsuleRadius, BotCenter - Fwd   * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter + Right * CapsuleRadius, BotCenter + Right * CapsuleRadius, ShapeColor);
	Scene.AddDebugLine(TopCenter - Right * CapsuleRadius, BotCenter - Right * CapsuleRadius, ShapeColor);
}

void UCapsuleComponent::UpdateWorldAABB() const {
	FVector WorldCenter = GetWorldLocation();
	FVector Up = GetUpVector().Normalized();

	float BiasedHeight = CapsuleHalfHeight > CapsuleRadius ? (CapsuleHalfHeight - CapsuleRadius) : 0.f;

	FVector Extent(
		fabsf(Up.X) * BiasedHeight + CapsuleRadius,
		fabsf(Up.Y) * BiasedHeight + CapsuleRadius,
		fabsf(Up.Z) * BiasedHeight + CapsuleRadius
	);

	WorldAABBMinLocation = WorldCenter - Extent;
	WorldAABBMaxLocation = WorldCenter + Extent;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

bool UCapsuleComponent::LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult)
{
	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FMatrix& WorldInverse = GetWorldInverseMatrix();

	FRay LocalRay;
	LocalRay.Origin = WorldInverse.TransformPositionWithW(Ray.Origin);
	LocalRay.Direction = WorldInverse.TransformVector(Ray.Direction).Normalized();

	float ClosestT = FLT_MAX;
	const float BodyHalfHeight = CapsuleHalfHeight > CapsuleRadius ? CapsuleHalfHeight - CapsuleRadius : 0.0f;

	auto AcceptT = [&](float T)
	{
		if (T >= 0.0f && T < ClosestT)
		{
			ClosestT = T;
		}
	};

	const float A = LocalRay.Direction.X * LocalRay.Direction.X + LocalRay.Direction.Y * LocalRay.Direction.Y;
	const float B = 2.0f * (LocalRay.Origin.X * LocalRay.Direction.X + LocalRay.Origin.Y * LocalRay.Direction.Y);
	const float C = LocalRay.Origin.X * LocalRay.Origin.X + LocalRay.Origin.Y * LocalRay.Origin.Y - CapsuleRadius * CapsuleRadius;
	const float Discriminant = B * B - 4.0f * A * C;
	if (std::abs(A) >= 1e-8f && Discriminant >= 0.0f)
	{
		const float SqrtDiscriminant = std::sqrt(Discriminant);
		const float InvDenom = 1.0f / (2.0f * A);
		const float T0 = (-B - SqrtDiscriminant) * InvDenom;
		const float T1 = (-B + SqrtDiscriminant) * InvDenom;
		const float Z0 = LocalRay.Origin.Z + LocalRay.Direction.Z * T0;
		const float Z1 = LocalRay.Origin.Z + LocalRay.Direction.Z * T1;
		if (Z0 >= -BodyHalfHeight && Z0 <= BodyHalfHeight)
		{
			AcceptT(T0);
		}
		if (Z1 >= -BodyHalfHeight && Z1 <= BodyHalfHeight)
		{
			AcceptT(T1);
		}
	}

	auto IntersectSphere = [&](const FVector& Center)
	{
		const FVector ToRay = LocalRay.Origin - Center;
		const float SphereA = LocalRay.Direction.Dot(LocalRay.Direction);
		const float SphereB = 2.0f * ToRay.Dot(LocalRay.Direction);
		const float SphereC = ToRay.Dot(ToRay) - CapsuleRadius * CapsuleRadius;
		const float SphereDiscriminant = SphereB * SphereB - 4.0f * SphereA * SphereC;
		if (SphereDiscriminant < 0.0f || std::abs(SphereA) < 1e-8f)
		{
			return;
		}

		const float SqrtDiscriminant = std::sqrt(SphereDiscriminant);
		const float InvDenom = 1.0f / (2.0f * SphereA);
		AcceptT((-SphereB - SqrtDiscriminant) * InvDenom);
		AcceptT((-SphereB + SqrtDiscriminant) * InvDenom);
	};

	IntersectSphere(FVector(0.0f, 0.0f, BodyHalfHeight));
	IntersectSphere(FVector(0.0f, 0.0f, -BodyHalfHeight));

	if (ClosestT == FLT_MAX)
	{
		return false;
	}

	const FVector LocalHit = LocalRay.Origin + LocalRay.Direction * ClosestT;
	const FVector WorldHit = WorldMatrix.TransformPositionWithW(LocalHit);
	OutHitResult.HitComponent = this;
	OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHit);
	OutHitResult.WorldHitLocation = WorldHit;
	OutHitResult.bHit = true;
	return true;
}
