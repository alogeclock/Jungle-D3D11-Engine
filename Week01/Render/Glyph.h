#pragma once
struct FGlyph
{
	int Id = 0;          // 'A' = 65
	int X = 0;  //BitmapFont 내 x좌표
	int Y = 0;  //BitmapFont 내 y좌표
	int Width = 0; //Width when Rendering
	int Height = 0; //Height when Rendering
	int XOffset = 0; // ?? cursor기준 얼마나 옆으로 이동할지? 커서가뭐임 bearing은 뭐임
	int YOffset = 0; // ?? baseline기준 vertical offset
	int XAdvance = 0;//문자 그리고 cursor어느정도 이동할건지
	int Page = 0; //BitmapFont Image가 여러개면 사용
	int Channel = 0; //RGBA몇개 채널인지, 여기서는 어차피 다 씀
};