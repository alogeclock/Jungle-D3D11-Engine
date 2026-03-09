#include "PopupManager.h"
#include "Popup.h"

void FPopupManager::AddPopup(const std::string& Id, std::unique_ptr<IPopup> Popup)
{
	Popups[Id] = std::move(Popup);
}

void FPopupManager::Open(const std::string& Id)
{
	auto It = Popups.find(Id);
	if (It != Popups.end())
	{
		It->second->Open();
	}
}

void FPopupManager::Close(const std::string& Id)
{
	auto It = Popups.find(Id);
	if (It != Popups.end())
	{
		It->second->Close();
	}
}

void FPopupManager::CloseAll()
{
	for (auto& Pair : Popups)
	{
		Pair.second->Close();
	}
}

bool FPopupManager::IsOpen(const std::string& Id) const
{
	auto It = Popups.find(Id);
	if (It != Popups.end())
	{
		return It->second->IsOpen();
	}
	return false;
}

IPopup* FPopupManager::GetPopup(const std::string& Id)
{
	auto It = Popups.find(Id);
	if (It != Popups.end())
	{
		return It->second.get();
	}
	return nullptr;
}

const IPopup* FPopupManager::GetPopup(const std::string& Id) const
{
	auto It = Popups.find(Id);
	if (It != Popups.end())
	{
		return It->second.get();
	}
	return nullptr;
}

void FPopupManager::Update(FGameContext& Context)
{
	for (auto& Pair : Popups)
	{
		if (Pair.second->IsOpen())
		{
			Pair.second->Update(Context);
		}
	}
}

void FPopupManager::Render(FGameContext& Context)
{
	for (auto& Pair : Popups)
	{
		if (Pair.second->IsOpen())
		{
			Pair.second->Render(Context);
		}
	}
}
