#include "Input.h"

void FInput::Update()
{
}

bool FInput::GetKey(EKeyCode KeyCode) const
{
	return CurrentKeys[static_cast<int>(KeyCode)];
}

bool FInput::GetKeyDown(EKeyCode KeyCode) const
{
	int Index = static_cast<int>(KeyCode);
	return CurrentKeys[Index] && !PreviousKeys[Index];
}

bool FInput::GetKeyUp(EKeyCode KeyCode) const
{
	int Index = static_cast<int>(KeyCode);
	return !CurrentKeys[Index] && PreviousKeys[Index];
}
