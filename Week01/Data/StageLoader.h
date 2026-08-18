#pragma once

#include <string>
#include <vector>

struct FStageData;

class FStageLoader
{
  public:
    static FStageLoader &Get();

    bool Initialize(const std::string &Path);
    bool IsLoaded() const;

    int  GetStageCount() const;
    bool LoadStageById(int StageId, FStageData &OutStage) const;
    bool LoadEndingMessages(std::vector<std::string> &OutMessages) const;

  private:
    FStageLoader() = default;

  private:
    bool        bLoaded = false;
    std::string FileContent;
};
