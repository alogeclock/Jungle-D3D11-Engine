#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"

struct FVertexSimple
{
	float x, y, z;    // Position
	float r, g, b, a; // Color
};

#include "Sphere.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
};

class URenderer
{
public:
	struct FConstants
	{
		FVector Offset;
		float Scale;
		float Angle;
		float ChargeSign;
		float Pad[2];
	};

	// Direct3D 11 장치(Device)와 장치 컨텍스트(Device Context) 및 스왑 체인(Swap Chain)을 관리하기 위한 포인터들
	ID3D11Device* Device = nullptr; // GPU와 통신하기 위한 Direct3D 장치
	ID3D11DeviceContext* DeviceContext = nullptr; // GPU 명령 실행을 담당하는 컨텍스트
	IDXGISwapChain* SwapChain = nullptr; // 프레임 버퍼를 교체하는 데 사용되는 스왑 체인

	// 렌더링에 필요한 리소스 및 상태를 관리하기 위한 변수들
	ID3D11Texture2D* FrameBuffer = nullptr; // 화면 출력용 텍스처
	ID3D11RenderTargetView* FrameBufferRTV = nullptr; // 텍스처를 렌더 타겟으로 사용하는 뷰
	ID3D11RasterizerState* RasterizerState = nullptr; // 래스터라이저 상태(컬링, 채우기 모드 등 정의)
	ID3D11Buffer* ConstantBuffer = nullptr; // 쉐이더에 데이터를 전달하기 위한 상수 버퍼

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f }; // 화면을 초기화(clear)할 때 사용할 색상 (RGBA)
	D3D11_VIEWPORT ViewportInfo; // 렌더링 영역을 정의하는 뷰포트 정보

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

	void UpdateConstant(FVector Offset, float Scale = 1.0f, float Angle = 0.0f, float ChargeSign = 0.0f)
	{
		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;
			{
				constants->Offset = Offset;
				constants->Scale = Scale;
				constants->Angle = Angle;
				constants->ChargeSign = ChargeSign;
			}
			DeviceContext->Unmap(ConstantBuffer, 0);
		}
	}

	void Prepare()
	{
		DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		DeviceContext->RSSetViewports(1, &ViewportInfo);
		DeviceContext->RSSetState(RasterizerState);

		DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}

	void PrepareShader()
	{
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);

		if (ConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
			DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer);
		}
	}

	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
	{
		UINT offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);

		DeviceContext->Draw(numVertices, 0);
	}

	void CreateShader()
	{
		ID3DBlob* vertexshaderCSO;
		ID3DBlob* pixelshaderCSO;

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

		Stride = sizeof(FVertexSimple);

		vertexshaderCSO->Release();
		pixelshaderCSO->Release();
	}

	void ReleaseShader()
	{
		if (SimpleInputLayout)
		{
			SimpleInputLayout->Release();
			SimpleInputLayout = nullptr;
		}

		if (SimplePixelShader)
		{
			SimplePixelShader->Release();
			SimplePixelShader = nullptr;
		}

		if (SimpleVertexShader)
		{
			SimpleVertexShader->Release();
			SimpleVertexShader = nullptr;
		}
	}

public:
	// 렌더러 초기화 함수
	void Create(HWND hWindow)
	{
		// Direct3D 장치 및 스왑 체인 생성
		CreateDeviceAndSwapChain(hWindow);

		// 프레임 버퍼 생성
		CreateFrameBuffer();

		// 래스터라이저 상태 생성
		CreateRasterizerState();

		// 깊이 스텐실 버퍼 및 블렌드 상태는 이 코드에서는 다루지 않음
	}

	// Direct3D 장치 및 스왑 체인을 생성하는 함수
	void CreateDeviceAndSwapChain(HWND hWindow)
	{
		// 지원하는 Direct3D 기능 레벨을 정의
		D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

		// 스왑 체인 설정 구조체 초기화
		DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
		swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
		swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
		swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
		swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
		swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
		swapchaindesc.BufferCount = 2; // 더블 버퍼링
		swapchaindesc.OutputWindow = hWindow; // 렌더링할 창 핸들
		swapchaindesc.Windowed = TRUE; // 창 모드
		swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

		// Direct3D 장치와 스왑 체인을 생성
		D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
			featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
			&swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

		// 생성된 스왑 체인의 정보 가져오기
		SwapChain->GetDesc(&swapchaindesc);

		// 뷰포트 정보 설정
		ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
	}

	// Direct3D 장치 및 스왑 체인을 해제하는 함수
	void ReleaseDeviceAndSwapChain()
	{
		if (DeviceContext)
		{
			DeviceContext->Flush(); // 남아있는 GPU 명령 실행
		}

		if (SwapChain)
		{
			SwapChain->Release();
			SwapChain = nullptr;
		}

		if (Device)
		{
			Device->Release();
			Device = nullptr;
		}

		if (DeviceContext)
		{
			DeviceContext->Release();
			DeviceContext = nullptr;
		}
	}

	// 프레임 버퍼를 생성하는 함수
	void CreateFrameBuffer()
	{
		// 스왑 체인으로부터 백 버퍼 텍스처 가져오기
		SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

		// 렌더 타겟 뷰 생성
		D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
		framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // 색상 포맷
		framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // 2D 텍스처

		Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
	}

	// 프레임 버퍼를 해제하는 함수
	void ReleaseFrameBuffer()
	{
		if (FrameBuffer)
		{
			FrameBuffer->Release();
			FrameBuffer = nullptr;
		}

		if (FrameBufferRTV)
		{
			FrameBufferRTV->Release();
			FrameBufferRTV = nullptr;
		}
	}

	// 래스터라이저 상태를 생성하는 함수
	void CreateRasterizerState()
	{
		D3D11_RASTERIZER_DESC rasterizerdesc = {};
		rasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
		rasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링

		Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
	}

	// 래스터라이저 상태를 해제하는 함수
	void ReleaseRasterizerState()
	{
		if (RasterizerState)
		{
			RasterizerState->Release();
			RasterizerState = nullptr;
		}
	}

	// 렌더러에 사용된 모든 리소스를 해제하는 함수
	void Release()
	{
		RasterizerState->Release();

		// 렌더 타겟을 초기화
		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		ReleaseFrameBuffer();
		ReleaseDeviceAndSwapChain();
	}

	// 스왑 체인의 백 버퍼와 프론트 버퍼를 교체하여 화면에 출력
	void SwapBuffer()
	{
		SwapChain->Present(1, 0); // 1: VSync 활성화
	}

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
	{
		// 2. Create a vertex buffer
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

		ID3D11Buffer* vertexBuffer;

		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		return vertexBuffer;
	}

	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
	{
		vertexBuffer->Release();
	}

	void CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC constantbufferdesc = {};
		constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
		constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
		constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
	}

	void ReleaseConstantBuffer()
	{
		if (ConstantBuffer)
		{
			ConstantBuffer->Release();
			ConstantBuffer = nullptr;
		}
	}
};

class UPrimitive
{
public:
	virtual ~UPrimitive() {}
	virtual void Update(float deltaTime) = 0;
	virtual void Render(URenderer& renderer) = 0;
	virtual bool CheckCollision(UPrimitive* other) = 0;
	virtual void ResolveCollision(UPrimitive* other) = 0;
};

class UBall : public UPrimitive
{
public:
	// 필수 변수 (이름 변경 불가)
	FVector Location;
	FVector Velocity;
	float Radius;
	float Mass;
	static int TotalNumBalls;

	// 공유 렌더링 리소스 (버텍스 버퍼 1개만 사용)
	static ID3D11Buffer* SharedVertexBuffer;
	static UINT SharedVertexCount;

	// 각속도 관련
	float Angle;
	float AngularVelocity;
	float Inertia;

	static bool bUseAngularVelocity;
	static float FrictionCoeff;

	// 자력 관련
	float MagneticCharge;

	static bool bUseMagnetism;
	static float MagneticStrength;
	static float MinMagneticDist;

	// 반발 계수
	static float Restitution;

	// 속도 클램프
	static float MaxSpeed;
	static float MaxAngularSpeed;

	// 핀볼 관련
	bool bReserveDestroy;

	UBall(float radius, float x, float y, float vx, float vy)
	{
		Radius = radius;
		Mass = radius; // 질량은 크기에 비례
		Location = FVector(x, y, 0.0f);
		Velocity = FVector(vx, vy, 0.0f);
		Angle = 0.0f;
		AngularVelocity = 0.0f;
		Inertia = 0.4f * Mass * Radius * Radius; // (2/5) * m * r^2
		MagneticCharge = (rand() % 2 == 0) ? 1.0f : -1.0f; // N극 또는 S극
		bReserveDestroy = false;
		TotalNumBalls++;
	}

	~UBall()
	{
		TotalNumBalls--;
	}

	// A: 매 프레임 물리 업데이트 (위치 이동 + 벽 충돌)
	void Update(float deltaTime) override
	{
		// 속도를 위치에 반영
		Location.x += Velocity.x * deltaTime;
		Location.y += Velocity.y * deltaTime;

		// 각속도가 켜져 있을 때만 각도 갱신
		if (bUseAngularVelocity)
			Angle += AngularVelocity * deltaTime;

		// 벽 마찰 계산용 유효 질량 (1/mEff = 1/m + R^2/I)
		float mEff = 1.0f / (1.0f / Mass + Radius * Radius / Inertia);

		// 왼쪽 벽
		if (Location.x - Radius < -1.0f)
		{
			float vn = fabsf(Velocity.x);
			Location.x = -1.0f + Radius;
			Velocity.x = Restitution * fabsf(Velocity.x);

			if (bUseAngularVelocity && vn > 0.0f)
			{
				// 접촉점 (-R, 0), 접선 (0, 1)
				// 표면 접선속도: Vy - ω*R
				float vt = Velocity.y - AngularVelocity * Radius;
				float jt = mEff * vt;
				float jtMax = FrictionCoeff * 2.0f * vn * Mass;
				if (jt > jtMax) jt = jtMax;
				if (jt < -jtMax) jt = -jtMax;
				Velocity.y -= jt / Mass;
				AngularVelocity -= Radius * jt / Inertia;
			}
		}
		// 오른쪽 벽
		if (Location.x + Radius > 1.0f)
		{
			float vn = fabsf(Velocity.x);
			Location.x = 1.0f - Radius;
			Velocity.x = -Restitution * fabsf(Velocity.x);

			if (bUseAngularVelocity && vn > 0.0f)
			{
				// 접촉점 (R, 0), 접선 (0, 1)
				// 표면 접선속도: Vy + ω*R
				float vt = Velocity.y + AngularVelocity * Radius;
				float jt = mEff * vt;
				float jtMax = FrictionCoeff * 2.0f * vn * Mass;
				if (jt > jtMax) jt = jtMax;
				if (jt < -jtMax) jt = -jtMax;
				Velocity.y -= jt / Mass;
				AngularVelocity += Radius * jt / Inertia;
			}
		}
		// 아래쪽 벽
		if (Location.y - Radius < -1.0f)
		{
			float vn = fabsf(Velocity.y);
			Location.y = -1.0f + Radius;
			Velocity.y = Restitution * fabsf(Velocity.y);

			if (bUseAngularVelocity && vn > 0.0f)
			{
				// 접촉점 (0, -R), 접선 (1, 0)
				// 표면 접선속도: Vx + ω*R
				float vt = Velocity.x + AngularVelocity * Radius;
				float jt = mEff * vt;
				float jtMax = FrictionCoeff * 2.0f * vn * Mass;
				if (jt > jtMax) jt = jtMax;
				if (jt < -jtMax) jt = -jtMax;
				Velocity.x -= jt / Mass;
				AngularVelocity += Radius * jt / Inertia;
			}
		}
		// 위쪽 벽
		if (Location.y + Radius > 1.0f)
		{
			float vn = fabsf(Velocity.y);
			Location.y = 1.0f - Radius;
			Velocity.y = -Restitution * fabsf(Velocity.y);

			if (bUseAngularVelocity && vn > 0.0f)
			{
				// 접촉점 (0, R), 접선 (1, 0)
				// 표면 접선속도: Vx - ω*R
				float vt = Velocity.x - AngularVelocity * Radius;
				float jt = mEff * vt;
				float jtMax = FrictionCoeff * 2.0f * vn * Mass;
				if (jt > jtMax) jt = jtMax;
				if (jt < -jtMax) jt = -jtMax;
				Velocity.x -= jt / Mass;
				AngularVelocity -= Radius * jt / Inertia;
			}
		}
	}

	// B: 렌더링
	void Render(URenderer& renderer) override
	{
		float angle = bUseAngularVelocity ? Angle : 0.0f;
		float chargeSign = bUseMagnetism ? MagneticCharge : 0.0f;
		renderer.UpdateConstant(Location, Radius, angle, chargeSign);
		renderer.RenderPrimitive(SharedVertexBuffer, SharedVertexCount);
	}

	// C: 충돌 감지
	bool CheckCollision(UPrimitive* other) override
	{
		UBall* b = (UBall*)other;
		float dx = Location.x - b->Location.x;
		float dy = Location.y - b->Location.y;
		float distSq = dx * dx + dy * dy;
		float minDist = Radius + b->Radius;
		return distSq < minDist * minDist;
	}

	// D: 탄성 충돌 처리
	void ResolveCollision(UPrimitive* other) override
	{
		UBall* b = (UBall*)other;

		float dx = b->Location.x - Location.x;
		float dy = b->Location.y - Location.y;
		float dist = sqrtf(dx * dx + dy * dy);
		if (dist == 0.0f) return;

		// 충돌 법선 벡터 (this → other)
		float nx = dx / dist;
		float ny = dy / dist;

		// 상대 속도
		float dvx = Velocity.x - b->Velocity.x;
		float dvy = Velocity.y - b->Velocity.y;

		// 법선 방향 상대 속도
		float vn = dvx * nx + dvy * ny;

		// 이미 분리 중이면 처리하지 않음
		if (vn <= 0.0f) return;

		// ===== 법선 방향: 탄성 충돌 임펄스 (e = 1) =====
		float jn = (1.0f + Restitution) * vn / (1.0f / Mass + 1.0f / b->Mass);

		Velocity.x -= (jn / Mass) * nx;
		Velocity.y -= (jn / Mass) * ny;
		b->Velocity.x += (jn / b->Mass) * nx;
		b->Velocity.y += (jn / b->Mass) * ny;

		// ===== 접선 방향: 각속도 ON일 때만 =====
		if (bUseAngularVelocity)
		{
			float tx = -ny;
			float ty = nx;

			// 접촉점에서의 표면속도 계산
			// Ball1 접촉점: r1 = (R1*nx, R1*ny), ω1 × r1 = (-ω1*R1*ny, ω1*R1*nx)
			float surf1x = Velocity.x + (-AngularVelocity * Radius * ny);
			float surf1y = Velocity.y + (AngularVelocity * Radius * nx);
			// Ball2 접촉점: r2 = (-R2*nx, -R2*ny), ω2 × r2 = (ω2*R2*ny, -ω2*R2*nx)
			float surf2x = b->Velocity.x + (b->AngularVelocity * b->Radius * ny);
			float surf2y = b->Velocity.y + (-b->AngularVelocity * b->Radius * nx);

			// 상대 접선 표면속도
			float relSurfVt = (surf1x - surf2x) * tx + (surf1y - surf2y) * ty;

			// 접선 방향 유효 질량
			// |r1 × t| = R1, |r2 × t| = R2 (단위 벡터이므로)
			float denom = 1.0f / Mass + 1.0f / b->Mass
				+ Radius * Radius / Inertia
				+ b->Radius * b->Radius / b->Inertia;
			float jt = relSurfVt / denom;

			// 쿨롱 마찰 제한
			float jtMax = FrictionCoeff * jn;
			if (jt > jtMax) jt = jtMax;
			if (jt < -jtMax) jt = -jtMax;

			// 선속도 접선 성분 반영
			Velocity.x -= (jt / Mass) * tx;
			Velocity.y -= (jt / Mass) * ty;
			b->Velocity.x += (jt / b->Mass) * tx;
			b->Velocity.y += (jt / b->Mass) * ty;

			// 각속도 반영 (토크 = r × 임펄스)
			AngularVelocity -= jt * Radius / Inertia;
			b->AngularVelocity -= jt * b->Radius / b->Inertia;
		}

		// 겹침 보정 (질량에 반비례하게 분리)
		float overlap = (Radius + b->Radius) - dist;
		if (overlap > 0.0f)
		{
			float totalMass = Mass + b->Mass;
			Location.x -= (overlap * b->Mass / totalMass) * nx;
			Location.y -= (overlap * b->Mass / totalMass) * ny;
			b->Location.x += (overlap * Mass / totalMass) * nx;
			b->Location.y += (overlap * Mass / totalMass) * ny;
		}
	}
};

int UBall::TotalNumBalls = 0;
ID3D11Buffer* UBall::SharedVertexBuffer = nullptr;
UINT UBall::SharedVertexCount = 0;
bool UBall::bUseAngularVelocity = false;
float UBall::FrictionCoeff = 0.3f;
bool UBall::bUseMagnetism = false;
float UBall::MagneticStrength = 0.02f;
float UBall::MinMagneticDist = 0.05f;
float UBall::Restitution = 0.8f;
float UBall::MaxSpeed = 3.0f;
float UBall::MaxAngularSpeed = 20.0f;

float RandomFloat(float minVal, float maxVal)
{
	return minVal + (float)(rand() % 10000) / 10000.0f * (maxVal - minVal);
}

void AddBall(UPrimitive**& list, int& count)
{
	UPrimitive** newList = new UPrimitive * [count + 1];
	for (int i = 0; i < count; i++)
		newList[i] = list[i];

	float radius = RandomFloat(0.03f, 0.08f);
	float x = RandomFloat(-1.0f + radius, 1.0f - radius);
	float y = RandomFloat(-1.0f + radius, 1.0f - radius);
	float vx = RandomFloat(-0.5f, 0.5f);
	float vy = RandomFloat(-0.5f, 0.5f);

	newList[count] = new UBall(radius, x, y, vx, vy);

	if (list) delete[] list;
	list = newList;
	count++;
}

void RemoveBall(UPrimitive**& list, int& count)
{
	if (count <= 0) return;

	// 임의의 공을 선택하여 소멸
	int idx = rand() % count;
	delete list[idx];

	// 나머지 요소를 앞으로 이동
	for (int i = idx; i < count - 1; i++)
		list[i] = list[i + 1];

	count--;

	if (count == 0)
	{
		delete[] list;
		list = nullptr;
	}
	else
	{
		UPrimitive** newList = new UPrimitive * [count];
		for (int i = 0; i < count; i++)
			newList[i] = list[i];
		delete[] list;
		list = newList;
	}
}

class PinballGame
{
public:
	FVertexSimple left_wall_vertices[6];
	FVertexSimple right_wall_vertices[6];
	FVertexSimple left_wing_vertices[6];
	FVertexSimple right_wing_vertices[6];

	ID3D11Buffer* leftWallVB;
	ID3D11Buffer* rightWallVB;
	ID3D11Buffer* leftWingVB;
	ID3D11Buffer* rightWingVB;

	FVector leftWallA, leftWallB;
	FVector rightWallA, rightWallB;

	FVector leftPivot, rightPivot;
	float leftWingAngle, rightWingAngle;
	float prevLeftAngle, prevRightAngle;
	float wingRestAngle, wingMaxAngle;
	float wingSpeed, wingLength;

	bool bActive;

	PinballGame()
		: leftWallVB(nullptr), rightWallVB(nullptr)
		, leftWingVB(nullptr), rightWingVB(nullptr)
		, leftWingAngle(0), rightWingAngle(0)
		, prevLeftAngle(0), prevRightAngle(0)
		, wingRestAngle(0.5f), wingMaxAngle(0.5f)
		, wingSpeed(15.0f), wingLength(0.22f)
		, bActive(false)
	{}

	void BuildQuad(FVertexSimple* out,
		float ax, float ay, float bx, float by,
		float thickness, float r, float g, float b)
	{
		float dx = bx - ax, dy = by - ay;
		float len = sqrtf(dx * dx + dy * dy);
		if (len == 0.0f) return;
		float px = (-dy / len) * thickness;
		float py = (dx / len) * thickness;

		float cx = ax + px, cy = ay + py;
		float ddx = bx + px, ddy = by + py;

		// D3D11: CW in NDC = front face (FrontCounterClockwise=FALSE + viewport y-flip)
		float cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
		if (cross <= 0.0f)
		{
			// cross <= 0 → already CW in NDC → front face
			out[0] = { ax, ay, 0, r, g, b, 1 };
			out[1] = { bx, by, 0, r, g, b, 1 };
			out[2] = { cx, cy, 0, r, g, b, 1 };
			out[3] = { bx, by, 0, r, g, b, 1 };
			out[4] = { ddx, ddy, 0, r, g, b, 1 };
			out[5] = { cx, cy, 0, r, g, b, 1 };
		}
		else
		{
			// cross > 0 → CCW → reverse to CW
			out[0] = { ax, ay, 0, r, g, b, 1 };
			out[1] = { cx, cy, 0, r, g, b, 1 };
			out[2] = { bx, by, 0, r, g, b, 1 };
			out[3] = { bx, by, 0, r, g, b, 1 };
			out[4] = { cx, cy, 0, r, g, b, 1 };
			out[5] = { ddx, ddy, 0, r, g, b, 1 };
		}
	}

	void Init(URenderer& renderer)
	{
		leftWallA = FVector(-1.0f, -0.8f, 0.0f);
		leftWallB = FVector(-0.3f, -0.9f, 0.0f);
		rightWallA = FVector(1.0f, -0.8f, 0.0f);
		rightWallB = FVector(0.3f, -0.9f, 0.0f);

		float wt = 0.04f;
		BuildQuad(left_wall_vertices,
			leftWallA.x, leftWallA.y, leftWallB.x, leftWallB.y,
			-wt, 0.5f, 0.5f, 0.55f);
		BuildQuad(right_wall_vertices,
			rightWallA.x, rightWallA.y, rightWallB.x, rightWallB.y,
			wt, 0.5f, 0.5f, 0.55f);

		float L = wingLength;
		float h = 0.018f;

		// Left wing: +x 방향 (CW winding in NDC = front face)
		left_wing_vertices[0] = { 0, -h, 0, 1.0f, 0.7f, 0.2f, 1.0f };
		left_wing_vertices[1] = { 0,  h, 0, 1.0f, 0.7f, 0.2f, 1.0f };
		left_wing_vertices[2] = { L, -h, 0, 1.0f, 0.85f, 0.3f, 1.0f };
		left_wing_vertices[3] = { L, -h, 0, 1.0f, 0.85f, 0.3f, 1.0f };
		left_wing_vertices[4] = { 0,  h, 0, 1.0f, 0.7f, 0.2f, 1.0f };
		left_wing_vertices[5] = { L,  h, 0, 1.0f, 0.85f, 0.3f, 1.0f };

		// Right wing: -x 방향 (CW winding in NDC = front face)
		right_wing_vertices[0] = {  0, -h, 0, 1.0f, 0.7f, 0.2f, 1.0f };
		right_wing_vertices[1] = { -L, -h, 0, 1.0f, 0.85f, 0.3f, 1.0f };
		right_wing_vertices[2] = {  0,  h, 0, 1.0f, 0.7f, 0.2f, 1.0f };
		right_wing_vertices[3] = { -L, -h, 0, 1.0f, 0.85f, 0.3f, 1.0f };
		right_wing_vertices[4] = { -L,  h, 0, 1.0f, 0.85f, 0.3f, 1.0f };
		right_wing_vertices[5] = {  0,  h, 0, 1.0f, 0.7f, 0.2f, 1.0f };

		leftPivot = FVector(-0.3f, -0.9f, 0.0f);
		rightPivot = FVector(0.3f, -0.9f, 0.0f);
		leftWingAngle = -wingRestAngle;
		rightWingAngle = wingRestAngle;
		prevLeftAngle = leftWingAngle;
		prevRightAngle = rightWingAngle;

		leftWallVB = renderer.CreateVertexBuffer(left_wall_vertices, sizeof(left_wall_vertices));
		rightWallVB = renderer.CreateVertexBuffer(right_wall_vertices, sizeof(right_wall_vertices));
		leftWingVB = renderer.CreateVertexBuffer(left_wing_vertices, sizeof(left_wing_vertices));
		rightWingVB = renderer.CreateVertexBuffer(right_wing_vertices, sizeof(right_wing_vertices));

		bActive = false;
	}

	void Release(URenderer& renderer)
	{
		if (leftWallVB)  { renderer.ReleaseVertexBuffer(leftWallVB);  leftWallVB = nullptr; }
		if (rightWallVB) { renderer.ReleaseVertexBuffer(rightWallVB); rightWallVB = nullptr; }
		if (leftWingVB)  { renderer.ReleaseVertexBuffer(leftWingVB);  leftWingVB = nullptr; }
		if (rightWingVB) { renderer.ReleaseVertexBuffer(rightWingVB); rightWingVB = nullptr; }
	}

	void UpdateFlippers(float deltaTime)
	{
		if (!bActive) return;

		prevLeftAngle = leftWingAngle;
		prevRightAngle = rightWingAngle;

		bool leftOn = (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('Z') & 0x8000);
		float leftTarget = leftOn ? wingMaxAngle : -wingRestAngle;
		if (leftWingAngle < leftTarget)
		{
			leftWingAngle += wingSpeed * deltaTime;
			if (leftWingAngle > leftTarget) leftWingAngle = leftTarget;
		}
		else if (leftWingAngle > leftTarget)
		{
			leftWingAngle -= wingSpeed * deltaTime;
			if (leftWingAngle < leftTarget) leftWingAngle = leftTarget;
		}

		bool rightOn = (GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('M') & 0x8000);
		float rightTarget = rightOn ? -wingMaxAngle : wingRestAngle;
		if (rightWingAngle < rightTarget)
		{
			rightWingAngle += wingSpeed * deltaTime;
			if (rightWingAngle > rightTarget) rightWingAngle = rightTarget;
		}
		else if (rightWingAngle > rightTarget)
		{
			rightWingAngle -= wingSpeed * deltaTime;
			if (rightWingAngle < rightTarget) rightWingAngle = rightTarget;
		}
	}

	static void ResolveBallLine(UBall* ball, FVector A, FVector B)
	{
		float abx = B.x - A.x, aby = B.y - A.y;
		float acx = ball->Location.x - A.x, acy = ball->Location.y - A.y;
		float abLenSq = abx * abx + aby * aby;
		if (abLenSq < 0.0001f) return;

		float t = (acx * abx + acy * aby) / abLenSq;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		float px = A.x + t * abx, py = A.y + t * aby;
		float dx = ball->Location.x - px, dy = ball->Location.y - py;
		float distSq = dx * dx + dy * dy;

		if (distSq < ball->Radius * ball->Radius && distSq > 0.0001f)
		{
			float dist = sqrtf(distSq);
			float nx = dx / dist, ny = dy / dist;

			ball->Location.x += nx * (ball->Radius - dist);
			ball->Location.y += ny * (ball->Radius - dist);

			float vn = ball->Velocity.x * nx + ball->Velocity.y * ny;
			if (vn < 0.0f)
			{
				ball->Velocity.x -= (1.0f + UBall::Restitution) * vn * nx;
				ball->Velocity.y -= (1.0f + UBall::Restitution) * vn * ny;
			}
		}
	}

	void ResolveBallFlipper(UBall* ball, FVector pivot, float angle, float pAngle,
		float deltaTime, float tipLocalX)
	{
		float cosA = cosf(angle), sinA = sinf(angle);
		FVector wA = pivot;
		FVector wB(pivot.x + tipLocalX * cosA, pivot.y + tipLocalX * sinA, 0.0f);

		float abx = wB.x - wA.x, aby = wB.y - wA.y;
		float acx = ball->Location.x - wA.x, acy = ball->Location.y - wA.y;
		float abLenSq = abx * abx + aby * aby;
		if (abLenSq < 0.0001f) return;

		float t = (acx * abx + acy * aby) / abLenSq;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		float px = wA.x + t * abx, py = wA.y + t * aby;
		float dx = ball->Location.x - px, dy = ball->Location.y - py;
		float distSq = dx * dx + dy * dy;

		if (distSq < ball->Radius * ball->Radius && distSq > 0.0001f)
		{
			float dist = sqrtf(distSq);
			float nx = dx / dist, ny = dy / dist;

			ball->Location.x += nx * (ball->Radius - dist);
			ball->Location.y += ny * (ball->Radius - dist);

			float omega = (deltaTime > 0.001f) ? (angle - pAngle) / deltaTime : 0.0f;
			float rx = px - pivot.x, ry = py - pivot.y;
			float cvx = -omega * ry, cvy = omega * rx;

			float relVn = (ball->Velocity.x - cvx) * nx + (ball->Velocity.y - cvy) * ny;
			if (relVn < 0.0f)
			{
				float impulse = -(1.0f + UBall::Restitution) * relVn;
				ball->Velocity.x += impulse * nx;
				ball->Velocity.y += impulse * ny;
			}
		}
	}

	void CheckBallCollisions(UBall* ball, float deltaTime)
	{
		if (!bActive) return;

		ResolveBallLine(ball, leftWallA, leftWallB);
		ResolveBallLine(ball, rightWallA, rightWallB);

		ResolveBallFlipper(ball, leftPivot, leftWingAngle, prevLeftAngle, deltaTime, wingLength);
		ResolveBallFlipper(ball, rightPivot, rightWingAngle, prevRightAngle, deltaTime, -wingLength);

		if(ball->Location.y - ball->Radius <= -1.0f)
		{
			ball->bReserveDestroy = true;
		}
	}

	bool RemoveDeadBalls(UPrimitive**& list, int& count)
	{
		if (!bActive) return false;
		bool bAnyRemoved = false;

		for (int i = count - 1; i >= 0; i--)
		{
			UBall* ball = (UBall*)list[i];
			if (ball->bReserveDestroy)
			{
				bAnyRemoved = true;
				delete ball;
				for (int j = i; j < count - 1; j++)
					list[j] = list[j + 1];
				count--;
			}
		}
		if (count == 0)
		{
			delete[] list;
			list = nullptr;
		}
		else
		{
			UPrimitive** newList = new UPrimitive * [count];
			for (int i = 0; i < count; i++)
				newList[i] = list[i];
			delete[] list;
			list = newList;
		}

		return bAnyRemoved;
	}

	void Render(URenderer& renderer)
	{
		if (!bActive) return;

		renderer.UpdateConstant(FVector(0, 0, 0), 1.0f, 0.0f, 0.0f);
		renderer.RenderPrimitive(leftWallVB, 6);
		renderer.RenderPrimitive(rightWallVB, 6);

		renderer.UpdateConstant(leftPivot, 1.0f, leftWingAngle, 0.0f);
		renderer.RenderPrimitive(leftWingVB, 6);

		renderer.UpdateConstant(rightPivot, 1.0f, rightWingAngle, 0.0f);
		renderer.RenderPrimitive(rightWingVB, 6);
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	srand(GetTickCount());

#pragma region 윈도우 생성
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };
	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);
#pragma endregion //윈도우 생성

#pragma region Renderer 및 ImGui 초기화
	URenderer renderer;
	renderer.Create(hWnd);
	renderer.CreateShader();
	renderer.CreateConstantBuffer();

	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);
#pragma endregion //Renderer 및 ImGui 초기화

	// 버텍스 버퍼 1개만 생성 (스케일링은 셰이더에서 처리)
	UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	ID3D11Buffer* vertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));

	// UBall 공유 렌더링 리소스 설정
	UBall::SharedVertexBuffer = vertexBufferSphere;
	UBall::SharedVertexCount = numVerticesSphere;

	// 핀볼 초기화
	PinballGame pinball;
	pinball.Init(renderer);

	// PrimitiveList 초기화
	UPrimitive** PrimitiveList = nullptr;
	int ballCount = 0;
	int desiredBallCount = 1;

	// 초기 공 1개 생성
	AddBall(PrimitiveList, ballCount);

	// 물리 설정
	bool bGravity = false;
	const float gravityAccel = 1.0f;

	// FPS 제한
	const int targetFPS = 60;
	const double targetFrameTime = 1000.0 / targetFPS;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER prevTime, startTime, endTime;
	QueryPerformanceCounter(&prevTime);
	double elapsedTime = 0.0;

	bool bIsExit = false;

	// ============================
	// 메인 루프
	// ============================
	while (bIsExit == false)
	{
		QueryPerformanceCounter(&startTime);
		float deltaTime = (float)(startTime.QuadPart - prevTime.QuadPart) / (float)frequency.QuadPart;
		prevTime = startTime;
		if (deltaTime > 0.05f) deltaTime = 0.05f;

		// ---- 메시지 처리 ----
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}

		// ---- ImGui 프레임 시작 ----
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Jungle Property Window");
		ImGui::Text("Total Balls: %d", UBall::TotalNumBalls);
		ImGui::InputInt("Num Balls", &desiredBallCount);
		if (desiredBallCount < 1) desiredBallCount = 1;
		ImGui::Checkbox("Gravity", &bGravity);
		ImGui::SliderFloat("Restitution", &UBall::Restitution, 0.0f, 1.0f);

		// 각속도 토글 + OFF 전환 시 리셋
		bool prevAngular = UBall::bUseAngularVelocity;
		ImGui::Checkbox("Angular Velocity", &UBall::bUseAngularVelocity);
		if (prevAngular && !UBall::bUseAngularVelocity)
		{
			for (int i = 0; i < ballCount; i++)
			{
				UBall* ball = (UBall*)PrimitiveList[i];
				ball->AngularVelocity = 0.0f;
				ball->Angle = 0.0f;
			}
		}
		if (UBall::bUseAngularVelocity)
		{
			ImGui::SliderFloat("Friction", &UBall::FrictionCoeff, 0.0f, 1.0f);
		}

		// 자력 토글
		ImGui::Checkbox("Magnetism", &UBall::bUseMagnetism);
		if (UBall::bUseMagnetism)
		{
			ImGui::SliderFloat("Magnetic Strength", &UBall::MagneticStrength, 0.001f, 0.1f);
			ImGui::SliderFloat("Min Distance", &UBall::MinMagneticDist, 0.01f, 0.2f);
		}

		// 핀볼 토글
		ImGui::Separator();
		bool bPrevPinball = pinball.bActive;
		ImGui::Checkbox("Pinball", &pinball.bActive);
		if (pinball.bActive)
		{
			ImGui::Text("  Left: Z / Left Arrow");
			ImGui::Text("  Right: M / Right Arrow");
		}

		if(bPrevPinball == false && pinball.bActive)
		{
			bGravity = true;
		}

		// 속도 클램프 슬라이더
		ImGui::Separator();
		ImGui::SliderFloat("Max Speed", &UBall::MaxSpeed, 0.5f, 10.0f);
		ImGui::SliderFloat("Max Angular Speed", &UBall::MaxAngularSpeed, 1.0f, 100.0f);

		if (ImGui::Button("Quit"))
		{
			PostMessage(hWnd, WM_QUIT, 0, 0);
		}
		ImGui::End();

		// ---- 공 개수 조정 ----
		while (ballCount < desiredBallCount)
			AddBall(PrimitiveList, ballCount);
		while (ballCount > desiredBallCount)
			RemoveBall(PrimitiveList, ballCount);

		if(pinball.RemoveDeadBalls(PrimitiveList, ballCount))
		{
			desiredBallCount = ballCount;
		}

		// ---- 플리퍼 업데이트 ----
		pinball.UpdateFlippers(deltaTime);

		// ---- 중력 적용 ----
		if (bGravity)
		{
			for (int i = 0; i < ballCount; i++)
			{
				UBall* ball = (UBall*)PrimitiveList[i];
				ball->Velocity.y -= gravityAccel * deltaTime;
			}
		}

		// ---- 자력 적용 (쿨롱 법칙: F = k * q1 * q2 / r²) ----
		if (UBall::bUseMagnetism)
		{
			float minDistSq = UBall::MinMagneticDist * UBall::MinMagneticDist;
			for (int i = 0; i < ballCount; i++)
			{
				UBall* a = (UBall*)PrimitiveList[i];
				for (int j = i + 1; j < ballCount; j++)
				{
					UBall* b = (UBall*)PrimitiveList[j];

					float dx = b->Location.x - a->Location.x;
					float dy = b->Location.y - a->Location.y;
					float distSq = dx * dx + dy * dy;

					// 최소 거리 제한 (무한대 힘 방지)
					if (distSq < minDistSq)
						distSq = minDistSq;

					float dist = sqrtf(distSq);
					float nx = dx / dist;
					float ny = dy / dist;

					// 같은 부호 → 양수 → 반발, 다른 부호 → 음수 → 인력
					float force = UBall::MagneticStrength * a->MagneticCharge * b->MagneticCharge / distSq;

					// a에 작용 (-n 방향: 같은 극이면 밀려남)
					a->Velocity.x -= force * nx / a->Mass * deltaTime;
					a->Velocity.y -= force * ny / a->Mass * deltaTime;
					// b에 작용 (+n 방향: 반작용)
					b->Velocity.x += force * nx / b->Mass * deltaTime;
					b->Velocity.y += force * ny / b->Mass * deltaTime;
				}
			}
		}

		// ---- 물리 업데이트 (이동 + 벽 충돌) ----
		for (int i = 0; i < ballCount; i++)
		{
			PrimitiveList[i]->Update(deltaTime);
		}

		// ---- 공끼리 충돌 처리 ----
		for (int i = 0; i < ballCount; i++)
		{
			for (int j = i + 1; j < ballCount; j++)
			{
				if (PrimitiveList[i]->CheckCollision(PrimitiveList[j]))
				{
					PrimitiveList[i]->ResolveCollision(PrimitiveList[j]);
				}
			}
		}

		// ---- 핀볼 충돌 처리 ----
		for (int i = 0; i < ballCount; i++)
		{
			pinball.CheckBallCollisions((UBall*)PrimitiveList[i], deltaTime);
		}

		// ---- 속도 클램프 (물리 폭발 방지) ----
		for (int i = 0; i < ballCount; i++)
		{
			UBall* ball = (UBall*)PrimitiveList[i];

			// 선속도 클램프
			float speedSq = ball->Velocity.x * ball->Velocity.x + ball->Velocity.y * ball->Velocity.y;
			float maxSp = UBall::MaxSpeed;
			if (speedSq > maxSp * maxSp)
			{
				float speed = sqrtf(speedSq);
				ball->Velocity.x = ball->Velocity.x / speed * maxSp;
				ball->Velocity.y = ball->Velocity.y / speed * maxSp;
			}

			// 각속도 클램프
			if (ball->AngularVelocity > UBall::MaxAngularSpeed)
				ball->AngularVelocity = UBall::MaxAngularSpeed;
			if (ball->AngularVelocity < -UBall::MaxAngularSpeed)
				ball->AngularVelocity = -UBall::MaxAngularSpeed;
		}

		// ---- 렌더링 ----
		renderer.Prepare();
		renderer.PrepareShader();

		pinball.Render(renderer);

		for (int i = 0; i < ballCount; i++)
		{
			PrimitiveList[i]->Render(renderer);
		}

		// ---- ImGui 렌더링 ----
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		renderer.SwapBuffer();

		// ---- FPS 제한 ----
		do
		{
			Sleep(0);
			QueryPerformanceCounter(&endTime);
			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
		} while (elapsedTime < targetFrameTime);
	}

#pragma region Release
	for (int i = 0; i < ballCount; i++)
	{
		delete PrimitiveList[i];
	}
	if (PrimitiveList)
	{
		delete[] PrimitiveList;
		PrimitiveList = nullptr;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	pinball.Release(renderer);
	renderer.ReleaseVertexBuffer(vertexBufferSphere);
	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();
#pragma endregion

	return 0;
}
