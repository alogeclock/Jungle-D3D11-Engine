#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

class FAudioSystem
{
public:
    static FAudioSystem &Get();

    bool Initialize();
    void Shutdown();

    // WAV 파일 로드 (키로 관리)
    bool LoadWav(const std::string &Key, const std::string &FilePath);

    // 재생 (bLoop = true 이면 반복 재생)
    bool Play(const std::string &Key, bool bLoop = false);
    void Stop(const std::string &Key);
    void StopAll();

    // 볼륨 조절 (0.0 ~ 1.0)
    void SetVolume(const std::string &Key, float Volume);
    void SetMasterVolume(float Volume);

    // 재생 속도 조절 (1.0 = 원래 속도)
    void SetPlaybackRate(const std::string &Key, float Rate);
    void SetAllPlaybackRate(float Rate);

    bool IsPlaying(const std::string &Key) const;

private:
    FAudioSystem() = default;
    ~FAudioSystem();

    FAudioSystem(const FAudioSystem &) = delete;
    FAudioSystem &operator=(const FAudioSystem &) = delete;

    struct FWavData
    {
        std::vector<uint8_t>  AudioData;
        std::vector<uint8_t>  FormatData; // WAVEFORMATEX or WAVEFORMATEXTENSIBLE
        const WAVEFORMATEX   *GetFormat() const { return reinterpret_cast<const WAVEFORMATEX *>(FormatData.data()); }
    };

    struct FPlayingVoice
    {
        IXAudio2SourceVoice *Voice = nullptr;
        bool                 bIsPlaying = false;
    };

    IXAudio2              *XAudio2 = nullptr;
    IXAudio2MasteringVoice *MasterVoice = nullptr;

    std::unordered_map<std::string, FWavData>      LoadedSounds;
    std::unordered_map<std::string, FPlayingVoice> PlayingVoices;

    bool bInitialized = false;
};
