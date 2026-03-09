#include "CreditPopup.h"
#include "../Core/GameContext.h"

void FCreditPopup::Open()
{
	bIsOpen = true;
}

void FCreditPopup::Close()
{
	bIsOpen = false;
}

bool FCreditPopup::IsOpen() const
{
	return bIsOpen;
}

void FCreditPopup::Update(FGameContext& Context)
{
}

void FCreditPopup::Render(FGameContext& Context)
{
}
