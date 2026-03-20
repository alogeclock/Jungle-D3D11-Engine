#pragma once
#include <algorithm>
#include <float.h>
#include "Vector.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct FBox
{
    FVector<float> Min;
    FVector<float> Max;

    FBox() : Min(FLT_MAX, FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
    FBox(const FVector<float> &InMin, const FVector<float> &InMax) : Min(InMin), Max(InMax) {}

    FVector<float> GetCenter() const { return (Min + Max) * 0.5f; }
    FVector<float> GetExtents() const { return (Max - Min) * 0.5f; }

    // 다른 AABB 박스나 점을 포함하도록 영역을 확장시킨다.
    void Encapsulate(const FVector<float> &Point)
    {
        Min.X = MIN(Min.X, Point.X), Min.Y = MIN(Min.Y, Point.Y), Min.Z = MIN(Min.Z, Point.Z);
        Max.X = MAX(Max.X, Point.X), Max.Y = MAX(Max.Y, Point.Y), Max.Z = MAX(Max.Z, Point.Z);
    }
};