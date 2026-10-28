#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include "musdesk/core/AudioAnalysis.hpp"

namespace musdesk {
struct MediaTools {
    std::string ffmpegPath;
    std::string ffprobePath;
    std::string status;
};

struct AudioMetadata {
    std::string path;
    double durationSeconds;
};

struct DecodedAudio {
    static constexpr int sampleRate = 48000;
    static constexpr int channels = 2;
    std::string path;
    std::vector<std::int16_t> samples;
    AudioEnvelope envelope;
    AudioWaveform waveform;
    AudioRhythm rhythm;
    AudioSpectrum spectrum;
    struct SourceStamp {
        std::uintmax_t size;
        std::filesystem::file_time_type modified;
        bool operator==(const SourceStamp&) const = default;
    };
    std::optional<SourceStamp> sourceStamp;

    double durationSeconds() const {
        return static_cast<double>(samples.size() / channels) / sampleRate;
    }
    AudioReaction reactionAt(double seconds, float beatThreshold) const {
        return {envelope.at(seconds), rhythm.bass.at(seconds), rhythm.pulseAt(seconds, beatThreshold), spectrum.at(seconds)};
    }
};

// Searches the configured directory, executable directory, then PATH.
MediaTools detectMediaTools(std::stop_token cancellation = {});
bool isAudioPath(const std::string& path);
AudioMetadata importAudio(const std::string& path, const MediaTools& tools, std::stop_token cancellation = {});
DecodedAudio prepareAudio(const std::string& path, const MediaTools& tools, std::stop_token cancellation = {});
void validateAudioSource(const DecodedAudio& audio);
}
