#pragma once

#include <memory>

namespace musdesk {
struct DecodedAudio;

struct PlaybackState {
    double timeSeconds = 0;
    bool playing = false;
    bool ended = false;
};

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    void load(std::shared_ptr<const DecodedAudio> audio);
    void unload();
    void play();
    void pause();
    void seek(double seconds);
    void setVolume(float volume);
    PlaybackState state();
    bool loaded() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    float volume_ = 1;
};
}
