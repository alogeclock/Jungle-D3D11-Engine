#include "Texture.h"

#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs")

using Microsoft::WRL::ComPtr;

FTexture::~FTexture()
{
	Release();
}

bool FTexture::LoadFromFile(ID3D11Device* Device, const std::string& Path)
{
	Release();

	// 경로를 와이드 문자열로 변환
	int WideLen = MultiByteToWideChar(CP_UTF8, 0, Path.c_str(), -1, nullptr, 0);
	std::wstring WidePath(WideLen, 0);
	MultiByteToWideChar(CP_UTF8, 0, Path.c_str(), -1, &WidePath[0], WideLen);

	// WIC 팩토리 생성
	ComPtr<IWICImagingFactory> Factory;
	HRESULT Hr = CoCreateInstance(
		CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&Factory));
	if (FAILED(Hr)) return false;

	// 이미지 파일 디코딩
	ComPtr<IWICBitmapDecoder> Decoder;
	Hr = Factory->CreateDecoderFromFilename(
		WidePath.c_str(), nullptr, GENERIC_READ,
		WICDecodeMetadataCacheOnLoad, &Decoder);
	if (FAILED(Hr)) return false;

	ComPtr<IWICBitmapFrameDecode> Frame;
	Hr = Decoder->GetFrame(0, &Frame);
	if (FAILED(Hr)) return false;

	// RGBA 32비트로 변환
	ComPtr<IWICFormatConverter> Converter;
	Hr = Factory->CreateFormatConverter(&Converter);
	if (FAILED(Hr)) return false;

	Hr = Converter->Initialize(
		Frame.Get(), GUID_WICPixelFormat32bppRGBA,
		WICBitmapDitherTypeNone, nullptr, 0.0,
		WICBitmapPaletteTypeCustom);
	if (FAILED(Hr)) return false;

	UINT W = 0, H = 0;
	Converter->GetSize(&W, &H);
	Width = static_cast<int>(W);
	Height = static_cast<int>(H);

	// 픽셀 데이터 읽기
	UINT RowPitch = W * 4;
	std::vector<BYTE> Pixels(RowPitch * H);
	Hr = Converter->CopyPixels(nullptr, RowPitch, static_cast<UINT>(Pixels.size()), Pixels.data());
	if (FAILED(Hr)) return false;

	// D3D11 텍스처 생성
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = W;
	TexDesc.Height = H;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = Pixels.data();
	InitData.SysMemPitch = RowPitch;

	Hr = Device->CreateTexture2D(&TexDesc, &InitData, &Texture2D);
	if (FAILED(Hr)) return false;

	// SRV 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = TexDesc.Format;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Texture2D.MipLevels = 1;

	Hr = Device->CreateShaderResourceView(Texture2D, &SrvDesc, &SRV);
	if (FAILED(Hr))
	{
		Texture2D->Release();
		Texture2D = nullptr;
		return false;
	}

	return true;
}

void FTexture::Release()
{
	if (SRV) { SRV->Release(); SRV = nullptr; }
	if (Texture2D) { Texture2D->Release(); Texture2D = nullptr; }
	Width = 0;
	Height = 0;
}

int FTexture::GetWidth() const
{
	return Width;
}

int FTexture::GetHeight() const
{
	return Height;
}

ID3D11ShaderResourceView* FTexture::GetSRV() const
{
	return SRV;
}
