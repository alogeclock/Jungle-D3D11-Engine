#include "pch.h"
#include "SpriteAnimation.h"
#include "IO/JsonFile.h"
#include "Core/Logger.h"

void FSpriteAnimator::AddAnimation(const std::string &Name, const FSpriteAnimation &Anim)
{
    Animations[Name] = Anim;
}

void FSpriteAnimator::Play(const std::string &Name, bool bForceRestart)
{
    if (CurrentAnim == Name && !bForceRestart)
        return;

    auto It = Animations.find(Name);
    if (It == Animations.end())
        return;

    CurrentAnim = Name;
    CurrentFrame = 0;
    Timer = 0.0f;
    bFinished = false;
}

void FSpriteAnimator::Update(float DeltaTime)
{
    if (bFinished || CurrentAnim.empty())
        return;

    auto It = Animations.find(CurrentAnim);
    if (It == Animations.end() || It->second.Frames.empty())
        return;

    const FSpriteAnimation &Anim = It->second;
    Timer += DeltaTime;

    while (Timer >= Anim.Frames[CurrentFrame].Duration)
    {
        Timer -= Anim.Frames[CurrentFrame].Duration;
        CurrentFrame++;

        if (CurrentFrame >= static_cast<int>(Anim.Frames.size()))
        {
            if (Anim.bLoop)
            {
                CurrentFrame = 0;
            }
            else
            {
                CurrentFrame = static_cast<int>(Anim.Frames.size()) - 1;
                bFinished = true;
                Timer = 0.0f;
                return;
            }
        }
    }
}

const FKeyframe *FSpriteAnimator::GetCurrentKeyframe() const
{
    if (CurrentAnim.empty())
        return nullptr;

    auto It = Animations.find(CurrentAnim);
    if (It == Animations.end() || It->second.Frames.empty())
        return nullptr;

    return &It->second.Frames[CurrentFrame];
}

bool FSpriteAnimator::IsPlaying() const { return !bFinished && !CurrentAnim.empty(); }

bool FSpriteAnimator::IsFinished() const { return bFinished; }

const std::string &FSpriteAnimator::GetCurrentAnimName() const { return CurrentAnim; }

FSpriteAnimation MakeSheetAnimation(float FrameWidth, float FrameHeight,
                                     int FrameCount, float FrameDuration,
                                     bool bLoop, int StartRow)
{
    FSpriteAnimation Anim;
    Anim.bLoop = bLoop;
    Anim.Frames.reserve(FrameCount);

    for (int i = 0; i < FrameCount; ++i)
    {
        FKeyframe KF;
        KF.OffsetMin = {FrameWidth * i, FrameHeight * StartRow};
        KF.OffsetMax = {FrameWidth * (i + 1), FrameHeight * (StartRow + 1)};
        KF.Duration = FrameDuration;
        Anim.Frames.push_back(KF);
    }

    return Anim;
}

// JSON 포맷 예시:
// {
//   "animations": [
//     {
//       "name": "idle",
//       "loop": true,
//       "frames": [
//         { "offset_min": [0, 0], "offset_max": [96, 96], "duration": 0.15 },
//         { "offset_min": [96, 0], "offset_max": [192, 96], "duration": 0.15 }
//       ]
//     }
//   ]
// }
bool LoadAnimationsFromJson(const std::string &JsonPath, FSpriteAnimator &OutAnimator,
                             bool &OutDefaultMirrored)
{
    OutDefaultMirrored = false;

    FJsonFile File;
    if (!File.LoadFromFile(JsonPath))
        return false;

    const auto &Root = File.GetRoot();
    OutDefaultMirrored = Root.value("default_mirrored", false);

    if (!Root.contains("animations") || !Root["animations"].is_array())
        return false;

    for (const auto &AnimJson : Root["animations"])
    {
        std::string Name = AnimJson.value("name", "");
        if (Name.empty())
            continue;

        FSpriteAnimation Anim;
        Anim.bLoop = AnimJson.value("loop", true);

        if (!AnimJson.contains("frames") || !AnimJson["frames"].is_array())
            continue;

        for (const auto &FrameJson : AnimJson["frames"])
        {
            FKeyframe KF;

            if (FrameJson.contains("offset_min") && FrameJson["offset_min"].is_array() &&
                FrameJson["offset_min"].size() >= 2)
            {
                KF.OffsetMin.x = FrameJson["offset_min"][0].get<float>();
                KF.OffsetMin.y = FrameJson["offset_min"][1].get<float>();
            }

            if (FrameJson.contains("offset_max") && FrameJson["offset_max"].is_array() &&
                FrameJson["offset_max"].size() >= 2)
            {
                KF.OffsetMax.x = FrameJson["offset_max"][0].get<float>();
                KF.OffsetMax.y = FrameJson["offset_max"][1].get<float>();
            }

            KF.Duration = FrameJson.value("duration", 0.1f);
            Anim.Frames.push_back(KF);
        }

        if (!Anim.Frames.empty())
        {
            OutAnimator.AddAnimation(Name, Anim);
            Logger::Log("Animation loaded: " + Name + " (" +
                        std::to_string(Anim.Frames.size()) + " frames)");
        }
    }

    return true;
}
