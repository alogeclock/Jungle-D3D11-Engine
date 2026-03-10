#include "UIManager.h"

void FUIManager::AddWidget(const std::string& Id, std::unique_ptr<IUIWidget> Widget)
{
    WidgetMap[Id] = std::move(Widget);
}

void FUIManager::RemoveWidget(const std::string& Id)
{
    WidgetMap.erase(Id);
}

void FUIManager::ClearWidgets()
{
    WidgetMap.clear();
}

bool FUIManager::HasWidget(const std::string& Id) const
{
    return WidgetMap.find(Id) != WidgetMap.end();
}

IUIWidget* FUIManager::GetWidget(const std::string& Id)
{
    auto It = WidgetMap.find(Id);
    return It != WidgetMap.end() ? It->second.get() : nullptr;
}

const IUIWidget* FUIManager::GetWidget(const std::string& Id) const
{
    auto It = WidgetMap.find(Id);
    return It != WidgetMap.end() ? It->second.get() : nullptr;
}

void FUIManager::SetWidgetVisible(const std::string& Id, bool bVisible)
{
    if (auto* W = GetWidget(Id))
    {
        W->SetVisible(bVisible);
    }
}

bool FUIManager::IsWidgetVisible(const std::string& Id) const
{
    if (const auto* W = GetWidget(Id))
    {
        return W->IsVisible();
    }
    return false;
}

void FUIManager::Update(FGameContext& Context)
{
    for (auto& [Id, Widget] : WidgetMap)
    {
        if (Widget && Widget->IsVisible())
        {
            Widget->Update(Context);
        }
    }
    PopupManager.Update(Context);
}

void FUIManager::Render(FGameContext& Context)
{
    for (auto& [Id, Widget] : WidgetMap)
    {
        if (Widget && Widget->IsVisible())
        {
            Widget->Render(Context);
        }
    }
    PopupManager.Render(Context);
}

FPopupManager& FUIManager::GetPopupManager()
{
    return PopupManager;
}

const FPopupManager& FUIManager::GetPopupManager() const
{
    return PopupManager;
}

void FUIManager::ClearAll()
{
    WidgetMap.clear();
    PopupManager.CloseAll();
}
