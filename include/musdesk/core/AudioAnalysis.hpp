#pragma once

#include <cstdint>
#include <span>
#include <stop_token>
#include <vector>

namespace musdesk {
struct AudioEnvelope {
    double stepSeconds = 0.02;
    double durationSeconds = 0;
    std::vector<float> values;

    float at(double timeSeconds) const;
};

AudioEnvelope analyzeRms(std::span<const std::int16_t> samples, int sampleRate, int channels,
    std::stop_token cancellation = {});

struct WaveformPeak {
    float minimum = 0;
    float maximum = 0;
};

struct AudioWaveform {
    std::vector<WaveformPeak> peaks;
};

AudioWaveform analyzeWaveform(std::span<const std::int16_t> samples, int channels,
    std::stop_token cancellation = {});

struct BeatEvent {
    double timeSeconds = 0;
    float strength = 0;
    bool operator==(const BeatEvent&) const = default;
};

struct AudioRhythm {
    AudioEnvelope bass;
    std::vector<BeatEvent> beats;
    float pulseAt(double timeSeconds) const;
};

AudioRhythm analyzeRhythm(std::span<const std::int16_t> samples, int sampleRate, int channels,
    std::stop_token cancellation = {});
}
