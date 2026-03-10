#pragma once

#include <string>
#include <vector>
#include "Core/Types.h"

struct FStageData;

class FStageLoader
{
  public:
    static FStageLoader &Get();

    bool Initialize(const std::string &Path);
    bool IsLoaded() const;

    int  GetStageCount() const;
    bool LoadStageById(int StageIndex, FStageData &OutStage) const;

  private:
    FStageLoader() = default;

    std::string FileContent;
    bool        bLoaded = false;
};
