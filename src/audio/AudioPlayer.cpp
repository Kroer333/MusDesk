#include "musdesk/audio/AudioPlayer.hpp"
#include "musdesk/media/AudioImporter.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace musdesk {
namespace {
void check(bool success) {
    if (!success) throw std::runtime_error(std::string("Audio preview: ") + SDL_GetError());
}
struct StreamLock {
    SDL_AudioStream* stream;
    explicit StreamLock(SDL_AudioStream* value) : stream(value) { check(SDL_LockAudioStream(stream)); }
    ~StreamLock() { SDL_UnlockAudioStream(stream); }
};
}

struct AudioPlayer::Impl {
    std::shared_ptr<const DecodedAudio> audio;
    SDL_AudioStream* stream = nullptr;
    std::size_t submittedFrames = 0;
    bool playing = false;
    bool failed = false;
    bool flushed = false;

    ~Impl() { if (stream) SDL_DestroyAudioStream(stream); }

    static void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional, int) {
        auto& self = *static_cast<Impl*>(userdata);
        if (!self.playing || self.failed || additional <= 0) return;
        constexpr std::size_t bytesPerFrame = DecodedAudio::channels * sizeof(std::int16_t);
        const auto total = self.audio->samples.size() / DecodedAudio::channels;
        const auto requested = (static_cast<std::size_t>(additional) + bytesPerFrame - 1) / bytesPerFrame;
        const auto frames = std::min(requested, total - self.submittedFrames);
        if (frames) {
            if (!SDL_PutAudioStreamData(stream, self.audio->samples.data() + self.submittedFrames * DecodedAudio::channels,
                    static_cast<int>(frames * bytesPerFrame))) {
                self.failed = true;
                return;
            }
            self.submittedFrames += frames;
        }
        if (self.submittedFrames == total && !self.flushed) {
            self.failed = !SDL_FlushAudioStream(stream);
            self.flushed = true;
        }
    }

    double position() const {
        const int queued = SDL_GetAudioStreamQueued(stream);
        if (queued < 0 || failed) throw std::runtime_error("Audio preview could not supply samples to the output device.");
        const auto pending = static_cast<std::size_t>(queued) / (DecodedAudio::channels * sizeof(std::int16_t));
        const auto consumed = submittedFrames - std::min(submittedFrames, pending);
        return static_cast<double>(consumed) / DecodedAudio::sampleRate;
    }
};

AudioPlayer::AudioPlayer() = default;
AudioPlayer::~AudioPlayer() = default;

void AudioPlayer::load(std::shared_ptr<const DecodedAudio> audio) {
    if (!audio || audio->samples.empty() || audio->samples.size() % DecodedAudio::channels != 0)
        throw std::runtime_error("No complete PCM audio frames to play.");
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) check(SDL_InitSubSystem(SDL_INIT_AUDIO));
    auto replacement = std::make_unique<Impl>();
    replacement->audio = std::move(audio);
    const SDL_AudioSpec format{SDL_AUDIO_S16, DecodedAudio::channels, DecodedAudio::sampleRate};
    replacement->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, Impl::feed, replacement.get());
    check(replacement->stream != nullptr);
    check(SDL_SetAudioStreamGain(replacement->stream, volume_));
    impl_.swap(replacement);
}

bool AudioPlayer::loaded() const { return impl_ != nullptr; }
void AudioPlayer::unload() { impl_.reset(); }

void AudioPlayer::play() {
    if (!impl_) return;
    const auto current = state();
    if (current.ended) seek(0);
    {
        StreamLock lock(impl_->stream);
        impl_->playing = true;
    }
    try { check(SDL_ResumeAudioStreamDevice(impl_->stream)); }
    catch (...) {
        StreamLock lock(impl_->stream);
        impl_->playing = false;
        throw;
    }
}

void AudioPlayer::pause() {
    if (!impl_) return;
    check(SDL_PauseAudioStreamDevice(impl_->stream));
    StreamLock lock(impl_->stream);
    impl_->playing = false;
}

void AudioPlayer::seek(double seconds) {
    if (!impl_) return;
    const bool resume = state().playing;
    pause();
    {
        StreamLock lock(impl_->stream);
        check(SDL_ClearAudioStream(impl_->stream));
        const double time = std::isfinite(seconds) ? std::clamp(seconds, 0.0, impl_->audio->durationSeconds()) : 0;
        const auto total = impl_->audio->samples.size() / DecodedAudio::channels;
        impl_->submittedFrames = std::min(total, static_cast<std::size_t>(std::llround(time * DecodedAudio::sampleRate)));
        impl_->flushed = false;
        impl_->failed = false;
    }
    if (resume && seconds < impl_->audio->durationSeconds()) play();
}

void AudioPlayer::setVolume(float volume) {
    volume_ = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0;
    if (impl_) check(SDL_SetAudioStreamGain(impl_->stream, volume_));
}

PlaybackState AudioPlayer::state() {
    if (!impl_) return {};
    PlaybackState result;
    {
        StreamLock lock(impl_->stream);
        result.timeSeconds = impl_->position();
        result.playing = impl_->playing;
        result.ended = result.timeSeconds >= impl_->audio->durationSeconds();
    }
    if (result.ended && result.playing) {
        pause();
        result.playing = false;
    }
    return result;
}
}
