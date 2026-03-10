#include "pch.h"
#include "Popup.h"
#include "PopupManager.h"

void FPopupManager::AddPopup(const std::string &Id, std::unique_ptr<IPopup> Popup)
{
    PopupMap[Id] = std::move(Popup);
}

void FPopupManager::Open(const std::string &Id)
{
    auto It = PopupMap.find(Id);
    if (It != PopupMap.end())
    {
        It->second->Open();
    }
}

void FPopupManager::Close(const std::string &Id)
{
    auto It = PopupMap.find(Id);
    if (It != PopupMap.end())
    {
        It->second->Close();
    }
}

void FPopupManager::CloseAll()
{
    for (auto &Pair : PopupMap)
    {
        Pair.second->Close();
    }
}

bool FPopupManager::IsOpen(const std::string &Id) const
{
    auto It = PopupMap.find(Id);
    if (It != PopupMap.end())
    {
        return It->second->IsOpen();
    }

    return false;
}

IPopup *FPopupManager::GetPopup(const std::string &Id)
{
    auto It = PopupMap.find(Id);
    if (It != PopupMap.end())
    {
        return It->second.get();
    }

    return nullptr;
}

const IPopup *FPopupManager::GetPopup(const std::string &Id) const
{
    auto It = PopupMap.find(Id);
    if (It != PopupMap.end())
    {
        return It->second.get();
    }

    return nullptr;
}

void FPopupManager::Update(FGameContext &Context)
{
    for (auto &Pair : PopupMap)
    {
        if (Pair.second->IsOpen())
        {
            Pair.second->Update(Context);
        }
    }
}

void FPopupManager::Render(FGameContext &Context)
{
    for (auto &Pair : PopupMap)
    {
        if (Pair.second->IsOpen())
        {
            Pair.second->Render(Context);
        }
    }
}
