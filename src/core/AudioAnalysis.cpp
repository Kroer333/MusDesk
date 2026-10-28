#include "musdesk/core/AudioAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace musdesk {
AudioWaveform analyzeWaveform(std::span<const std::int16_t> samples, int channels,
    std::stop_token cancellation) {
    if (channels < 1 || channels > 8 || samples.empty() || samples.size() % channels != 0)
        throw std::runtime_error("Invalid PCM audio for waveform analysis.");
    const auto frames = samples.size() / channels;
    const auto count = std::min<std::size_t>(8192, frames);
    AudioWaveform result;
    result.peaks.reserve(count);
    for (std::size_t bucket = 0; bucket < count; ++bucket) {
        if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
        const auto begin = bucket * frames / count;
        const auto end = (bucket + 1) * frames / count;
        auto minimum = samples[begin * channels];
        auto maximum = minimum;
        for (auto i = begin * channels; i < end * channels; ++i) {
            minimum = std::min(minimum, samples[i]);
            maximum = std::max(maximum, samples[i]);
        }
        result.peaks.push_back({minimum / 32768.0f, maximum / 32768.0f});
    }
    return result;
}

float AudioEnvelope::at(double timeSeconds) const {
    if (values.empty() || !std::isfinite(timeSeconds) || timeSeconds < 0
        || timeSeconds >= durationSeconds || !(stepSeconds > 0)) return 0;
    const double position = timeSeconds / stepSeconds;
    const auto index = static_cast<std::size_t>(std::min(position, static_cast<double>(values.size() - 1)));
    const auto next = std::min(index + 1, values.size() - 1);
    const float fraction = static_cast<float>(std::clamp(position - index, 0.0, 1.0));
    return std::lerp(values[index], values[next], fraction);
}

AudioEnvelope analyzeRms(std::span<const std::int16_t> samples, int sampleRate, int channels,
    std::stop_token cancellation) {
    if (sampleRate < 50 || channels < 1 || channels > 8 || samples.empty() || samples.size() % channels != 0)
        throw std::runtime_error("Invalid PCM audio for RMS analysis.");
    const std::size_t frames = samples.size() / channels;
    const std::size_t window = static_cast<std::size_t>(sampleRate / 50);
    AudioEnvelope result;
    result.stepSeconds = static_cast<double>(window) / sampleRate;
    result.durationSeconds = static_cast<double>(frames) / sampleRate;
    result.values.push_back(0);
    float peak = 0.05f;
    for (std::size_t begin = 0; begin < frames; begin += window) {
        if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
        const auto end = std::min(begin + window, frames);
        double energy = 0;
        for (auto i = begin * channels; i < end * channels; ++i) {
            const double value = samples[i] / 32768.0;
            energy += value * value;
        }
        const auto rms = static_cast<float>(std::sqrt(energy / ((end - begin) * channels)));
        result.values.push_back(rms);
        peak = std::max(peak, rms);
    }
    float smoothed = 0;
    const float attack = static_cast<float>(1 - std::exp(-result.stepSeconds / 0.03));
    const float release = static_cast<float>(1 - std::exp(-result.stepSeconds / 0.18));
    for (auto& value : result.values) {
        const float target = value < 0.0001f ? 0 : std::clamp(value / peak, 0.0f, 1.0f);
        smoothed += (target - smoothed) * (target > smoothed ? attack : release);
        value = smoothed;
    }
    return result;
}
}
