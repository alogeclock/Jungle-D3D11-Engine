#pragma once

#include <d3d11.h>
#include <string>

class FTexture
{
public:
	~FTexture();

	// WIC를 사용하여 이미지 파일에서 텍스처 생성 (PNG, BMP, JPG 지원)
	bool LoadFromFile(ID3D11Device* Device, const std::string& Path);
	void Release();

	int GetWidth() const;
	int GetHeight() const;

	ID3D11ShaderResourceView* GetSRV() const;
	ID3D11Texture2D* Texture2D = nullptr;
	ID3D11ShaderResourceView* SRV = nullptr;

private:
	int Width = 0;
	int Height = 0;

};
