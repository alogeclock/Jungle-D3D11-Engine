#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "./popup/PopupManager.h"
#include "./widget/UIWidget.h"

struct FGameContext;

class FUIManager
{
  public:
    void AddWidget(const std::string &Id, std::unique_ptr<IUIWidget> Widget);

    void RemoveWidget(const std::string &Id);
    void ClearWidgets();

    bool HasWidget(const std::string &Id) const;

    IUIWidget       *GetWidget(const std::string &Id);
    const IUIWidget *GetWidget(const std::string &Id) const;

    void SetWidgetVisible(const std::string &Id, bool bVisible);
    bool IsWidgetVisible(const std::string &Id) const;

    void Update(FGameContext &Context);
    void Render(FGameContext &Context);

    FPopupManager       &GetPopupManager();
    const FPopupManager &GetPopupManager() const;

    void ClearAll();

  private:
    std::unordered_map<std::string, std::unique_ptr<IUIWidget>> WidgetMap;
    FPopupManager                                               PopupManager;
};
