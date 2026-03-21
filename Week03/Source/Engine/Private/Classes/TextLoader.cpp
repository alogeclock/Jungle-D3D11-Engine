#include "Source/Engine/Public/Classes/TextLoader.h"

#include <fstream>
#include <sstream>
#include <iostream>

bool UTextLoader::LoadTextFromFile(const FString& InFilePath, FString& OutResult)
{
	// std::ifstream을 사용해 읽기 모드로 파일을 엽니다.
	// FString이 내부적으로 std::string과 동일하거나 변환을 지원한다고 가정합니다.
	std::ifstream FileStream(InFilePath.c_str(), std::ios::in);

	if (!FileStream.is_open())
	{
		// 프로젝트의 로그 시스템(예: FLogger)이 있다면 std::cout 대신 사용하는 것이 좋습니다.
		std::cout << "[CTextLoader] 오류: 텍스트 파일을 열 수 없습니다. 경로: " 
		          << InFilePath.c_str() << std::endl;
		return false;
	}

	// 파일의 전체 내용을 한 번에 효율적으로 읽어오기 위해 stringstream 사용
	std::stringstream StringStream;
	StringStream << FileStream.rdbuf();

	// 파일 핸들 닫기
	FileStream.close();

	// 버퍼에 담긴 내용을 OutResult로 복사
	OutResult = StringStream.str();
	
	return true;
}

bool UTextLoader::SaveTextToFile(const FString& InFilePath, const FString& InText)
{
	// std::ofstream을 사용해 쓰기 모드로 파일을 엽니다. (std::ios::trunc로 기존 내용 초기화)
	std::ofstream FileStream(InFilePath.c_str(), std::ios::out | std::ios::trunc);

	if (!FileStream.is_open())
	{
		std::cout << "[CTextLoader] 오류: 파일에 쓰기 위해 열 수 없습니다. 경로: " 
		          << InFilePath.c_str() << std::endl;
		return false;
	}

	// 문자열 데이터를 파일에 쓰기
	FileStream << InText.c_str();

	// 파일 핸들 닫기
	FileStream.close();

	return true;
}