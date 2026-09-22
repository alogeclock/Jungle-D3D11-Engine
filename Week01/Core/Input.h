#pragma once

#include "Types.h"

// todo: down -> press, up -> release로 용어 변경 고려

class FInput
{
  public:
    void Update();

    bool GetKey(EKeyCode KeyCode) const;
    bool GetKeyDown(EKeyCode KeyCode) const;
    bool GetKeyUp(EKeyCode KeyCode) const;

  private:
    bool CurrentKeys[static_cast<int>(EKeyCode::Count)]{};
    bool PreviousKeys[static_cast<int>(EKeyCode::Count)]{};
};
