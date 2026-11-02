#pragma once

#include <cstdint>
#include <array>
#include <span>
#include <stop_token>
#include <vector>

namespace musdesk {
struct AudioEnvelope {
    double stepSeconds = 0.02;
    double durationSeconds = 0;
    std::vector<float> values;

    float at(double timeSeconds) const;
    float filteredAt(double seconds, float attackMs, float releaseMs) const;
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
enum class BeatBand { Bass, Drums, Wide };
float beatLevelFromDb(double db);
double beatLevelDb(float level);

struct AudioRhythm {
    AudioEnvelope bass;
    std::vector<BeatEvent> beats;
    std::array<std::vector<BeatEvent>,2> otherBands;
    const std::vector<BeatEvent>& events(BeatBand band = BeatBand::Bass) const;
    float pulseAt(double timeSeconds, float minimumStrength = 0, BeatBand band = BeatBand::Bass) const;
    float kickAt(double timeSeconds, float minimumStrength = 0.55f, BeatBand band = BeatBand::Bass) const;
};

using SpectrumFrame = std::array<float, 32>;
SpectrumFrame spectrumForDisplay(const SpectrumFrame& spectrum, float gainDb, float balance);

struct AudioSpectrum {
    double stepSeconds = 0.02;
    double durationSeconds = 0;
    std::vector<SpectrumFrame> frames;
    SpectrumFrame at(double seconds) const;
    SpectrumFrame filteredAt(double seconds, float attackMs, float releaseMs) const;
};

AudioSpectrum analyzeSpectrum(std::span<const std::int16_t> samples, int sampleRate, int channels,
    std::stop_token cancellation = {});

struct AudioReaction {
    float energy = 0;
    float bass = 0;
    float beat = 0;
    SpectrumFrame spectrum{};
    float kick = 0;
    float glowEnergy = -1;
};

AudioRhythm analyzeRhythm(std::span<const std::int16_t> samples, int sampleRate, int channels,
    std::stop_token cancellation = {});
}
