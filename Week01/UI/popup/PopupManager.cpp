#include "pch.h"
#include "Core/GameContext.h"
#include "PopupManager.h"

void FPopupManager::Update(FGameContext &Context)
{
    if (!CurrentPopup)
        return;

    CurrentPopup->Update(Context);
}

void FPopupManager::Render(FGameContext &Context)
{
    if (!CurrentPopup)
        return;

    CurrentPopup->Render(Context);
}

bool FPopupManager::HasOpenPopup() const { return CurrentPopup != nullptr; }

bool FPopupManager::IsPopupClosed() const
{
    if (!CurrentPopup)
        return false;

    return CurrentPopup->IsClosed();
}

void FPopupManager::Open(std::unique_ptr<FUIPopupBase> InPopup)
{
    CurrentPopup = std::move(InPopup);
    if (CurrentPopup)
        CurrentPopup->Open();
}

void FPopupManager::RemoveClosedPopup()
{
    if (CurrentPopup && CurrentPopup->IsClosed())
    {
        CurrentPopup.reset();
    }
}
