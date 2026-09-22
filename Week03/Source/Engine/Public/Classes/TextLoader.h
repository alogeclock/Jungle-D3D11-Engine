#pragma once

#include "CoreTypes.h"
#include <string>

class UTextLoader
{
public:
	// 정적 유틸리티 클래스이므로 객체 생성을 원천적으로 방지합니다.
	UTextLoader() = delete;
	~UTextLoader() = delete;
	UTextLoader(const UTextLoader&) = delete;
	UTextLoader& operator=(const UTextLoader&) = delete;

	// 텍스트 파일을 읽어 OutResult에 문자열로 담아줍니다. (성공 시 true 반환)
	static bool LoadTextFromFile(const FString& InFilePath, FString& OutResult);

	// 텍스트 데이터를 지정한 파일 경로에 저장합니다. (기존 파일 덮어쓰기)
	static bool SaveTextToFile(const FString& InFilePath, const FString& InText);
};
