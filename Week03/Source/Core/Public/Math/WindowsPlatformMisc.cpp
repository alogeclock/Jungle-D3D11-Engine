#include "WindowsPlatformMisc.h"
#include "Guid.h"      // FGuid의 실제 구조를 알기 위해 포함
#include <objbase.h>   // CoCreateGuid와 GUID 구조체를 사용하기 위한 Windows 헤더
//#include "AutoRTFM.h"

void FWindowsPlatformMisc::CreateGuid(FGuid& Result)
{
	//if (AutoRTFM::IsTransactional())
	//{
	//	Result = AutoRTFM::Open([]
	//		{
	//			FGuid Guid;
	//			verify(CoCreateGuid((GUID*)&Guid) == S_OK);
	//			return Guid;
	//		});
	//}
	//else
	//{
	//	verify(CoCreateGuid((GUID*)&Result) == S_OK);
	//}



	// FGuid(uint32 4개, 총 16바이트)와 Windows API의 GUID(총 16바이트)는 
	// 메모리 크기가 동일하므로 포인터 캐스팅을 통해 바로 값을 채울 수 있습니다.
	HRESULT hr = CoCreateGuid((GUID*)&Result);

	// GUID 생성 성공 여부 확인 (S_OK)
	if (hr != S_OK)
	{
		// 만약 GUID 생성에 실패할 경우 안전하게 0으로 초기화합니다.
		Result = FGuid();
	}
}