#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "UIPopup.h"

struct FGameContext;

class FPopupManager
{
  public:
    void AddPopup(const std::string &Id, std::unique_ptr<IUIPopup> Popup);

    void Open(const std::string &Id);
    void Close(const std::string &Id);
    void CloseAll();

    bool IsOpen(const std::string &Id) const;
    bool HasOpenPopup() const; // PlayScene에서 pause 기능 넣고싶으면 사용

    IUIPopup       *GetPopup(const std::string &Id);
    const IUIPopup *GetPopup(const std::string &Id) const;

    void Update(FGameContext &Context);
    void Render(FGameContext &Context);

  private:
    std::unordered_map<std::string, std::unique_ptr<IUIPopup>> PopupMap;
};
