#pragma once

#include "../UI/Popup.h"

class FCreditPopup : public IPopup
{
public:
	void Open() override;
	void Close() override;
	bool IsOpen() const override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

private:
	bool bIsOpen = false;
};
