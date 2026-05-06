#pragma once
#include "Object/ObjectFactory.h"
#include "Vector.h"

class UCurveFloat :public UObject {
public:
	DECLARE_CLASS(UCurveFloat, UObject)
	TArray<FVector2> Curve;

public:
	FVector2 Evaluate(float NormalizedT) const;
};