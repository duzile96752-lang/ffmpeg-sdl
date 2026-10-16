#include "AudioPlayer.h"
#include<iostream>
#include"SDL3/SDL.h"

AudioPlayer::AudioPlayer()
{}

AudioPlayer::~AudioPlayer()
{
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool AudioPlayer::open(int sampleRate, int channels)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::cout<<"SDL_InitSubSystem(AUDIO) failed: "<< SDL_GetError()
            <<std::endl;
            return false;
    }
    sampleRate_ = sampleRate;
    channels_ = channels;

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = channels;
    spec.freq = sampleRate;

    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

    if (!stream_) {
        std::cout << "SDL_OpenAudioDeviceStream failed: "
            << SDL_GetError() << std::endl;
        return false;
    }

    SDL_ResumeAudioStreamDevice(stream_);
    return true; 
}

void AudioPlayer::push(const uint8_t* data, int size)
{
    if (!stream_ || size <= 0)return;
    SDL_PutAudioStreamData(stream_, data, size);
    pushedBytes_ += size;
}

void AudioPlayer::push(const std::vector<uint8_t>&data,double pts_)
{
    if (data.empty() || !stream_) return;

    if (pts_ >= 0.0) {
        lastPts_ = pts_;   // 记录这帧的 PTS
    }

    push(data.data(), (int)data.size());

}

int AudioPlayer::queueBytes() const
{
    if (!stream_) return 0;
    //获取队列中的元素
    return SDL_GetAudioStreamQueued(stream_);
}

double AudioPlayer::playedSeconds() const
{
    if (!stream_) return 0.0;

    int queuedBytes = SDL_GetAudioStreamQueued(stream_);
    double queuedSec = (double)queuedBytes / (sampleRate_ * channels_ * 2);

    // 已播放 = 最后 push 的 PTS − 队列里还剩的秒数
    double played = lastPts_ - queuedSec;
    return played > 0.0 ? played : 0.0;

    /*int64_t played = pushedBytes_ - queueBytes();
    if (played < 0)played = 0;
    int bytesPerSec = sampleRate_ * channels_ * 2;
    return (double)played / bytesPerSec;
    return 0.0;*/
}

double AudioPlayer::queueSeconds() const
{
    int bytesPerSec = sampleRate_ * channels_ * 2;
    return (double)queueBytes() / bytesPerSec;
}

void AudioPlayer::pause(bool b)
{
    if (!stream_)return;
    if (b)SDL_PauseAudioStreamDevice(stream_);
    else SDL_ResumeAudioStreamDevice(stream_);
}

void AudioPlayer::clear()
{
    if (!stream_)return;
    SDL_ClearAudioStream(stream_);
    pushedBytes_ = 0;
}
