#pragma once

#include <string>

class FMapData;

class FMapLoader
{
  public:
    bool LoadFromFile(FMapData &OutMap); // legacy, 삭제 예정
    bool LoadStageById(int StageId, FMapData &OutMap);
    int  GetStageCount();

  private:
    // TODO: 경로 외부 지정 혹은 상수로 빼두기
    static constexpr const char *DefaultPath = "Assets/Data/Maps.json";
    std::string                  Path = DefaultPath;
};
