#pragma once
#include <string>

enum class EItemType
{
    Invincibility,
    TimeScaleDown,
    DarknessUp,
    DarknessDown,
    TimeFreeze
};

struct FItemData
{
    int X = 0;
    int Y = 0;
    EItemType Type = EItemType::Invincibility;
    float Duration = 0.0f;  // 0 = permanent (darkness)
    int Level = 0;          // DarknessUp/Down: target level (1~5)
    bool bPickedUp = false;
};

struct FActiveEffect
{
    EItemType Type;
    float RemainingTime;
};

inline std::string GetItemTextureKey(EItemType Type)
{
    switch (Type)
    {
    case EItemType::Invincibility:  return "item_invincibility";
    case EItemType::TimeScaleDown:  return "item_time_scale_down";
    case EItemType::DarknessUp:     return "item_darkness_up";
    case EItemType::DarknessDown:   return "item_darkness_down";
    case EItemType::TimeFreeze:     return "item_time_freeze";
    default:                        return "item_invincibility";
    }
}
