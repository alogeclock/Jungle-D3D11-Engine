#pragma once

#include "Types.h"

class FInput
{
public:
	void Update();

	bool GetKey(EKeyCode KeyCode) const;
	bool GetKeyDown(EKeyCode KeyCode) const;
	bool GetKeyUp(EKeyCode KeyCode) const;

private:
	bool CurrentKeys[256]{};
	bool PreviousKeys[256]{};
};
