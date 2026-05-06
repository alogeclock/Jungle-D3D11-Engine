#include "CurveFloat.h"

IMPLEMENT_CLASS(UCurveFloat, UObject)

FVector2 UCurveFloat::Evaluate(float NormalizedT) const {
	FVector2 Out;
	bool AssertCondition = (NormalizedT >= 0 && NormalizedT <= 1);
	static_assert(&AssertCondition, "CurveFloat must be evaluated within the range T = [0, 1]");
	uint64 T = static_cast<uint64>(NormalizedT * Curve.size());


	return Out;
}