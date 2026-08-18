#pragma once

#include "ClothSimulationTypes.h"

/** Cloth Debug Draw 표시 옵션 */
struct FClothDebugDrawOptions
{
    bool bDrawParticles  = false;
    bool bDrawConstraints = false;
    bool bDrawPinned     = true;
    bool bDrawNormals    = false;
    bool bDrawBounds     = false;
};
