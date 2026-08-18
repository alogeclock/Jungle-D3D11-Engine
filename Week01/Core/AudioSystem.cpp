#include "pch.h"
#include "AudioSystem.h"
#include <fstream>

#pragma comment(lib, "xaudio2.lib")

FAudioSystem::~FAudioSystem()
{
    Shutdown();
}

bool FAudioSystem::Initialize()
{
    if (bInitialized)
        return true;

    HRESULT Hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(Hr) && Hr != RPC_E_CHANGED_MODE)
        return false;

    Hr = XAudio2Create(&XAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(Hr))
        return false;

    Hr = XAudio2->CreateMasteringVoice(&MasterVoice);
    if (FAILED(Hr))
    {
        XAudio2->Release();
        XAudio2 = nullptr;
        return false;
    }

    // Submix Voice 생성 (BGM / SFX)
    Hr = XAudio2->CreateSubmixVoice(&BgmSubmix, 2, 44100);
    if (FAILED(Hr))
        BgmSubmix = nullptr;

    Hr = XAudio2->CreateSubmixVoice(&SfxSubmix, 2, 44100);
    if (FAILED(Hr))
        SfxSubmix = nullptr;

    bInitialized = true;
    return true;
}

void FAudioSystem::Shutdown()
{
    if (!bInitialized)
        return;

    StopAll();

    if (BgmSubmix)
    {
        BgmSubmix->DestroyVoice();
        BgmSubmix = nullptr;
    }

    if (SfxSubmix)
    {
        SfxSubmix->DestroyVoice();
        SfxSubmix = nullptr;
    }

    if (MasterVoice)
    {
        MasterVoice->DestroyVoice();
        MasterVoice = nullptr;
    }

    if (XAudio2)
    {
        XAudio2->Release();
        XAudio2 = nullptr;
    }

    LoadedSounds.clear();
    bInitialized = false;
}

IXAudio2SubmixVoice *FAudioSystem::GetSubmixVoice(EAudioChannel Channel)
{
    return (Channel == EAudioChannel::BGM) ? BgmSubmix : SfxSubmix;
}

// RIFF WAV 파일 파싱
bool FAudioSystem::LoadWav(const std::string &Key, const std::string &FilePath)
{
    if (!bInitialized)
        return false;

    // 이미 로드됨
    if (LoadedSounds.count(Key))
        return true;

    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
        return false;

    // RIFF 헤더
    char ChunkId[4];
    File.read(ChunkId, 4);
    if (memcmp(ChunkId, "RIFF", 4) != 0)
        return false;

    uint32_t ChunkSize;
    File.read(reinterpret_cast<char *>(&ChunkSize), 4);

    char Format[4];
    File.read(Format, 4);
    if (memcmp(Format, "WAVE", 4) != 0)
        return false;

    std::vector<uint8_t> FormatData;
    std::vector<uint8_t> AudioData;
    bool bFoundFmt = false;
    bool bFoundData = false;

    while (File.good() && !(bFoundFmt && bFoundData))
    {
        char SubChunkId[4];
        uint32_t SubChunkSize;

        File.read(SubChunkId, 4);
        File.read(reinterpret_cast<char *>(&SubChunkSize), 4);

        if (!File.good())
            break;

        if (memcmp(SubChunkId, "fmt ", 4) == 0)
        {
            // fmt 청크 전체를 WAVEFORMATEX 구조체 크기 이상으로 읽기
            // SubChunkSize가 16이면 PCM, 18이면 cbSize 포함, 40이면 EXTENSIBLE
            size_t AllocSize = (SubChunkSize < sizeof(WAVEFORMATEX)) ? sizeof(WAVEFORMATEX) : SubChunkSize;
            FormatData.resize(AllocSize, 0);
            File.read(reinterpret_cast<char *>(FormatData.data()), SubChunkSize);

            // cbSize 설정 (16바이트 PCM인 경우 cbSize가 파일에 없음)
            auto *Fmt = reinterpret_cast<WAVEFORMATEX *>(FormatData.data());
            if (SubChunkSize == 16)
            {
                Fmt->cbSize = 0;
            }

            bFoundFmt = true;
        }
        else if (memcmp(SubChunkId, "data", 4) == 0)
        {
            AudioData.resize(SubChunkSize);
            File.read(reinterpret_cast<char *>(AudioData.data()), SubChunkSize);
            bFoundData = true;
        }
        else
        {
            // 알 수 없는 청크 스킵
            File.seekg(SubChunkSize, std::ios::cur);
        }
    }

    if (!bFoundFmt || !bFoundData || AudioData.empty())
        return false;

    FWavData Data;
    Data.FormatData = std::move(FormatData);
    Data.AudioData = std::move(AudioData);
    LoadedSounds[Key] = std::move(Data);

    return true;
}

bool FAudioSystem::Play(const std::string &Key, bool bLoop, EAudioChannel Channel)
{
    if (!bInitialized)
        return false;

    auto It = LoadedSounds.find(Key);
    if (It == LoadedSounds.end())
        return false;

    // 기존 재생 중이면 정지
    Stop(Key);

    const FWavData &Data = It->second;

    // Submix Voice로 라우팅
    IXAudio2SubmixVoice *Submix = GetSubmixVoice(Channel);
    XAUDIO2_SEND_DESCRIPTOR Send = {};
    Send.pOutputVoice = Submix;
    XAUDIO2_VOICE_SENDS SendList = {};
    SendList.SendCount = 1;
    SendList.pSends = &Send;

    IXAudio2SourceVoice *Voice = nullptr;
    HRESULT Hr = XAudio2->CreateSourceVoice(&Voice, Data.GetFormat(), 0, XAUDIO2_DEFAULT_FREQ_RATIO,
                                             nullptr, Submix ? &SendList : nullptr);
    if (FAILED(Hr))
        return false;

    XAUDIO2_BUFFER Buffer = {};
    Buffer.AudioBytes = static_cast<UINT32>(Data.AudioData.size());
    Buffer.pAudioData = Data.AudioData.data();
    Buffer.Flags = XAUDIO2_END_OF_STREAM;

    if (bLoop)
    {
        Buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    Hr = Voice->SubmitSourceBuffer(&Buffer);
    if (FAILED(Hr))
    {
        Voice->DestroyVoice();
        return false;
    }

    Voice->Start(0);

    FPlayingVoice PV;
    PV.Voice = Voice;
    PV.Channel = Channel;
    PV.bIsPlaying = true;
    PlayingVoices[Key] = PV;

    return true;
}

void FAudioSystem::Stop(const std::string &Key)
{
    auto It = PlayingVoices.find(Key);
    if (It != PlayingVoices.end())
    {
        if (It->second.Voice)
        {
            It->second.Voice->Stop(0);
            It->second.Voice->DestroyVoice();
        }
        PlayingVoices.erase(It);
    }
}

void FAudioSystem::StopAll()
{
    for (auto &Pair : PlayingVoices)
    {
        if (Pair.second.Voice)
        {
            Pair.second.Voice->Stop(0);
            Pair.second.Voice->DestroyVoice();
        }
    }
    PlayingVoices.clear();
}

void FAudioSystem::StopChannel(EAudioChannel Channel)
{
    for (auto It = PlayingVoices.begin(); It != PlayingVoices.end(); )
    {
        if (It->second.Channel == Channel)
        {
            if (It->second.Voice)
            {
                It->second.Voice->Stop(0);
                It->second.Voice->DestroyVoice();
            }
            It = PlayingVoices.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

void FAudioSystem::SetVolume(const std::string &Key, float Volume)
{
    auto It = PlayingVoices.find(Key);
    if (It != PlayingVoices.end() && It->second.Voice)
    {
        It->second.Voice->SetVolume(Volume);
    }
}

void FAudioSystem::SetMasterVolume(float Volume)
{
    if (MasterVoice)
    {
        MasterVoice->SetVolume(Volume);
    }
}

float FAudioSystem::GetMasterVolume() const
{
    float Volume = 1.0f;
    if (MasterVoice)
        MasterVoice->GetVolume(&Volume);
    return Volume;
}

void FAudioSystem::SetChannelVolume(EAudioChannel Channel, float Volume)
{
    IXAudio2SubmixVoice *Submix = GetSubmixVoice(Channel);
    if (Submix)
    {
        Submix->SetVolume(Volume);
    }
}

float FAudioSystem::GetChannelVolume(EAudioChannel Channel) const
{
    float Volume = 1.0f;
    IXAudio2SubmixVoice *Submix =
        (Channel == EAudioChannel::BGM) ? BgmSubmix : SfxSubmix;
    if (Submix)
        Submix->GetVolume(&Volume);
    return Volume;
}

void FAudioSystem::SetPlaybackRate(const std::string &Key, float Rate)
{
    auto It = PlayingVoices.find(Key);
    if (It != PlayingVoices.end() && It->second.Voice)
    {
        It->second.Voice->SetFrequencyRatio(Rate);
    }
}

void FAudioSystem::SetAllPlaybackRate(float Rate)
{
    for (auto &Pair : PlayingVoices)
    {
        if (Pair.second.Voice)
        {
            Pair.second.Voice->SetFrequencyRatio(Rate);
        }
    }
}

void FAudioSystem::SetChannelPlaybackRate(EAudioChannel Channel, float Rate)
{
    for (auto &Pair : PlayingVoices)
    {
        if (Pair.second.Channel == Channel && Pair.second.Voice)
        {
            Pair.second.Voice->SetFrequencyRatio(Rate);
        }
    }
}

bool FAudioSystem::IsPlaying(const std::string &Key) const
{
    auto It = PlayingVoices.find(Key);
    if (It == PlayingVoices.end() || !It->second.Voice)
        return false;

    XAUDIO2_VOICE_STATE State;
    It->second.Voice->GetState(&State);
    return State.BuffersQueued > 0;
}
