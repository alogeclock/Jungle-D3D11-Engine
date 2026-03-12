#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Singleton.h"

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;
struct IXAudio2SubmixVoice;

enum class EAudioChannel
{
    BGM,
    SFX
};

class FAudioSystem : public TSingleton<FAudioSystem>
{
    friend class TSingleton<FAudioSystem>;

  public:
    ~FAudioSystem();

    bool Initialize();
    void Shutdown();

    // WAV 파일 로드 (키로 관리)
    bool LoadWav(const std::string &Key, const std::string &FilePath);

    // 재생 (Channel로 BGM/SFX 구분)
    bool Play(const std::string &Key, bool bLoop = false,
              EAudioChannel Channel = EAudioChannel::SFX);
    void Stop(const std::string &Key);
    void StopAll();
    void StopChannel(EAudioChannel Channel);

    // 볼륨 조절 (0.0 ~ 1.0)
    void  SetVolume(const std::string &Key, float Volume);
    void  SetMasterVolume(float Volume);
    float GetMasterVolume() const;
    void  SetChannelVolume(EAudioChannel Channel, float Volume);
    float GetChannelVolume(EAudioChannel Channel) const;

    // 재생 속도 조절 (1.0 = 원래 속도)
    void SetPlaybackRate(const std::string &Key, float Rate);
    void SetAllPlaybackRate(float Rate);
    void SetChannelPlaybackRate(EAudioChannel Channel, float Rate);

    bool IsPlaying(const std::string &Key) const;

  private:
    struct FWavData
    {
        std::vector<uint8_t> AudioData;
        std::vector<uint8_t> FormatData; // WAVEFORMATEX or WAVEFORMATEXTENSIBLE
        const WAVEFORMATEX  *GetFormat() const
        {
            return reinterpret_cast<const WAVEFORMATEX *>(FormatData.data());
        }
    };

    struct FPlayingVoice
    {
        IXAudio2SourceVoice *Voice = nullptr;
        EAudioChannel        Channel = EAudioChannel::SFX;
        bool                 bIsPlaying = false;
    };

    IXAudio2SubmixVoice *GetSubmixVoice(EAudioChannel Channel);

    IXAudio2               *XAudio2 = nullptr;
    IXAudio2MasteringVoice *MasterVoice = nullptr;
    IXAudio2SubmixVoice    *BgmSubmix = nullptr;
    IXAudio2SubmixVoice    *SfxSubmix = nullptr;

    std::unordered_map<std::string, FWavData>      LoadedSounds;
    std::unordered_map<std::string, FPlayingVoice> PlayingVoices;

    bool bInitialized = false;
};
