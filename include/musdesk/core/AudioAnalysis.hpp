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
}
