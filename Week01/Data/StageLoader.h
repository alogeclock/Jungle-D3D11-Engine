#pragma once

#include <string>
#include "Core/Singleton.h"
#include "Core/Types.h"

struct FStageData;

class FStageLoader : public TSingleton<FStageLoader>
{
    friend class TSingleton<FStageLoader>;

  public:
    bool Initialize(const std::string &Path);
    bool IsLoaded() const;

    int  GetStageCount() const;
    bool LoadStageById(int StageIndex, FStageData &OutStage) const;

  private:
    std::string FileContent;
    bool        bLoaded = false;
};
