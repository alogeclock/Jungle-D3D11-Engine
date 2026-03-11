#pragma once

#include <DirectXMath.h>
#include <string>
#include <unordered_map>
#include <vector>

struct FKeyframe
{
    DirectX::XMFLOAT2 OffsetMin = {0.0f, 0.0f}; // 좌상단 픽셀 좌표
    DirectX::XMFLOAT2 OffsetMax = {0.0f, 0.0f}; // 우하단 픽셀 좌표
    float Duration = 0.1f;
};

struct FSpriteAnimation
{
    std::vector<FKeyframe> Frames;
    bool bLoop = true;
};

class FSpriteAnimator
{
  public:
    void AddAnimation(const std::string &Name, const FSpriteAnimation &Anim);
    void Play(const std::string &Name, bool bForceRestart = false);
    void Update(float DeltaTime);

    const FKeyframe *GetCurrentKeyframe() const;
    bool             IsPlaying() const;
    bool             IsFinished() const;
    const std::string &GetCurrentAnimName() const;

  private:
    std::unordered_map<std::string, FSpriteAnimation> Animations;
    std::string CurrentAnim;
    int         CurrentFrame = 0;
    float       Timer = 0.0f;
    bool        bFinished = false;
};

// 스프라이트 시트에서 가로로 나열된 프레임들로 애니메이션 생성 헬퍼
FSpriteAnimation MakeSheetAnimation(float FrameWidth, float FrameHeight,
                                     int FrameCount, float FrameDuration,
                                     bool bLoop = true, int StartRow = 0);

// JSON 파일에서 애니메이션 데이터를 로드하여 Animator에 등록
// OutDefaultMirrored: JSON의 "default_mirrored" 값 (없으면 false)
bool LoadAnimationsFromJson(const std::string &JsonPath, FSpriteAnimator &OutAnimator,
                             bool &OutDefaultMirrored);
