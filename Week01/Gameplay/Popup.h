#pragma once

struct FGameContext;

class IPopup
{
public:
	virtual ~IPopup() = default;

	virtual void Open() = 0;
	virtual void Close() = 0;
	virtual bool IsOpen() const = 0;

	virtual void Update(FGameContext& Context) = 0;
	virtual void Render(FGameContext& Context) = 0;
};
