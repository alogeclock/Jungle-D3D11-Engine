#pragma once

#include <memory>
#include <type_traits>
#include "Core/GameContext.h"
#include "UIPopupBase.h"

class FPopupManager
{
  public:
    void Update(FGameContext &Context);
    void Render(FGameContext &Context);

    bool HasOpenPopup() const;
    bool IsPopupClosed() const;

    void Open(std::unique_ptr<FUIPopupBase> InPopup);
    void RemoveClosedPopup();

    template <typename T>
    T *GetPopup()
    {
        static_assert(std::is_base_of_v<FUIPopupBase, T>, "T must derive from FUIPopupBase");
        return dynamic_cast<T *>(CurrentPopup.get());
    }

  private:
    std::unique_ptr<FUIPopupBase> CurrentPopup;
};
