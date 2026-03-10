#include "pch.h"
#include <Windows.h>
#include <cstddef>
#include "Input.h"

namespace
{
    int ToVirtualKey(EKeyCode KeyCode)
    {
        switch (KeyCode)
        {
        case EKeyCode::Up:
            return VK_UP;
        case EKeyCode::Down:
            return VK_DOWN;
        case EKeyCode::Left:
            return VK_LEFT;
        case EKeyCode::Right:
            return VK_RIGHT;
        default:
            return 0;
        }
    }
} // namespace

void FInput::Update()
{
    for (std::size_t Index = 0; Index < static_cast<std::size_t>(EKeyCode::Count); ++Index)
    {
        PreviousKeys[Index] = CurrentKeys[Index];
    }

    for (std::size_t Index = 0; Index < static_cast<std::size_t>(EKeyCode::Count); ++Index)
    {
        const EKeyCode KeyCode = static_cast<EKeyCode>(Index);
        const int      VirtualKey = ToVirtualKey(KeyCode);

        CurrentKeys[Index] = (GetAsyncKeyState(VirtualKey) & 0x8000) != 0;
    }
}

bool FInput::GetKey(EKeyCode KeyCode) const
{
    const std::size_t Index = static_cast<std::size_t>(KeyCode);
    return CurrentKeys[Index];
}

// Press
bool FInput::GetKeyDown(EKeyCode KeyCode) const
{
    const std::size_t Index = static_cast<std::size_t>(KeyCode);
    return CurrentKeys[Index] && !PreviousKeys[Index];
}

// Realease
bool FInput::GetKeyUp(EKeyCode KeyCode) const
{
    const std::size_t Index = static_cast<std::size_t>(KeyCode);
    return !CurrentKeys[Index] && PreviousKeys[Index];
}
