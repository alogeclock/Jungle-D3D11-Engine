#pragma once

#include <memory>
#include <type_traits>
#include "UIPopup.h"
#include "GameContext.h"

class FPopupManager
{
  public:
    void Update(FGameContext &Context);
    void Render(FGameContext &Context);

    bool HasOpenPopup() const;
    bool IsPopupClosed() const;

    void Open(std::unique_ptr<IUIPopup> InPopup);
    void RemoveClosedPopup();

    template <typename T>
    T *GetPopup()
    {
        static_assert(std::is_base_of_v<IUIPopup, T>, "T must derive from IUIPopup");
        return dynamic_cast<T *>(CurrentPopup.get());
    }

  private:
    std::unique_ptr<IUIPopup> CurrentPopup;
};
