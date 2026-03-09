#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Popup.h"
struct FGameContext;

class FPopupManager
{
public:
	void AddPopup(const std::string& Id, std::unique_ptr<IPopup> Popup);

	void Open(const std::string& Id);
	void Close(const std::string& Id);
	void CloseAll();

	bool IsOpen(const std::string& Id) const;

	IPopup* GetPopup(const std::string& Id);
	const IPopup* GetPopup(const std::string& Id) const;

	void Update(FGameContext& Context);
	void Render(FGameContext& Context);

private:
	std::unordered_map<std::string, std::unique_ptr<IPopup>> Popups;
};
