#pragma once

class FTexture
{
public:
	int GetWidth() const;
	int GetHeight() const;
	void* GetNativeHandle() const;

private:
	int Width = 0;
	int Height = 0;
	void* NativeHandle = nullptr;
};
