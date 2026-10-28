#include "musdesk/core/AudioAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void rejected(Action action) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Invalid audio analysis input was accepted");
}
void waveformTests() {
    std::vector<std::int16_t> silence(40000, 0);
    const auto quiet = musdesk::analyzeWaveform(silence, 2);
    require(quiet.peaks.size() == 8192, "Long waveforms must have bounded storage");
    require(std::all_of(quiet.peaks.begin(), quiet.peaks.end(), [](auto p) { return p.minimum == 0 && p.maximum == 0; }),
        "Silence must produce a flat waveform");
    const std::vector<std::int16_t> stereo{16384, -16384, 0, 32767, -32768, 0};
    const auto shortWave = musdesk::analyzeWaveform(stereo, 2);
    require(shortWave.peaks.size() == 3 && shortWave.peaks[0].minimum == -0.5f && shortWave.peaks[0].maximum == 0.5f,
        "Opposite-phase channels must not cancel in the waveform");
    require(shortWave.peaks.back().minimum == -1 && shortWave.peaks[1].maximum > 0.999f,
        "Signed PCM extrema must be preserved without overflow");
    silence.front() = 32767;
    silence[19999] = -32768;
    silence.back() = 16384;
    const auto peaks = musdesk::analyzeWaveform(silence, 1);
    require(peaks.peaks.front().maximum > 0.999f && peaks.peaks.back().maximum == 0.5f,
        "First and last samples must be retained");
    require(std::any_of(peaks.peaks.begin(), peaks.peaks.end(), [](auto p) { return p.minimum == -1; }),
        "Downsampling must retain a single-sample transient");
    require(std::all_of(peaks.peaks.begin(), peaks.peaks.end(), [](auto p) {
        return std::isfinite(p.minimum) && std::isfinite(p.maximum)
            && p.minimum >= -1 && p.maximum <= 1 && p.minimum <= p.maximum;
    }), "Waveform peaks must be ordered and finite");
    std::stop_source cancellation;
    cancellation.request_stop();
    rejected([&] { musdesk::analyzeWaveform(silence, 1, cancellation.get_token()); });
    rejected([&] { musdesk::analyzeWaveform({}, 2); });
    rejected([&] { musdesk::analyzeWaveform(stereo, 0); });
    rejected([&] { musdesk::analyzeWaveform(stereo, 9); });
    rejected([&] { musdesk::analyzeWaveform(std::span(stereo).first(5), 2); });
}
void rhythmTests() {
    constexpr int rate = 48000;
    const auto signal = [&](double frequency, bool sustained) {
        std::vector<std::int16_t> pcm(rate * 2 * 2, 0);
        for (int frame = 0; frame < rate * 2; ++frame) {
            const double time = static_cast<double>(frame) / rate;
            double amplitude = 0;
            for (double start : {0.3, 0.8, 1.3}) {
                const double age = time - start;
                if (age >= 0 && (sustained || age < 0.15))
                    amplitude = std::max(amplitude, sustained ? std::min(1.0, age / 0.01)
                        : std::min(1.0, age / 0.005) * std::exp(-age / 0.045));
            }
            const auto sample = static_cast<std::int16_t>(0.7 * 32767 * amplitude
                * std::sin(2 * std::numbers::pi * frequency * time));
            pcm[frame * 2] = sample;
            pcm[frame * 2 + 1] = -sample;
        }
        return pcm;
    };
    const auto pcm = signal(70, false);
    const auto rhythm = musdesk::analyzeRhythm(pcm, rate, 2);
    require(rhythm.beats.size() == 3, "Three separated bass bursts must produce three onsets");
    for (std::size_t i = 0; i < rhythm.beats.size(); ++i) {
        const auto& beat = rhythm.beats[i];
        require(std::abs(beat.timeSeconds - (0.3 + i * 0.5)) < 0.08, "Bass onset timing is outside tolerance");
        require(beat.strength > 0 && beat.strength <= 1, "Beat strengths must be normalized");
        require(rhythm.pulseAt(beat.timeSeconds) == beat.strength
            && rhythm.pulseAt(beat.timeSeconds + 0.1) < beat.strength, "Beat pulse must decay from the onset");
    }
    const auto high = musdesk::analyzeRhythm(signal(2000, false), rate, 2);
    require(high.beats.empty(), "High-frequency bursts must not trigger bass onsets");
    for (double frequency : {40.0, 70.0, 120.0, 180.0}) {
        require(musdesk::analyzeRhythm(signal(frequency, true), rate, 2).beats.size() == 1,
            "A sustained bass tone must not repeatedly trigger beats");
    }
    auto quietPcm = pcm;
    for (auto& sample : quietPcm) sample /= 100;
    require(musdesk::analyzeRhythm(quietPcm, rate, 2).beats.empty(), "Near-silence must not be amplified into beats");
    const auto silent = musdesk::analyzeRhythm(std::vector<std::int16_t>(rate * 2, 0), rate, 2);
    require(silent.beats.empty() && silent.bass.at(0.5) == 0 && silent.pulseAt(0.5) == 0,
        "Silence must not generate bass energy or beats");
    require(rhythm.pulseAt(0.1) == 0 && rhythm.pulseAt(-1) == 0 && rhythm.pulseAt(2) == 0
        && rhythm.pulseAt(std::numeric_limits<double>::quiet_NaN()) == 0, "Invalid or pre-onset times must have no pulse");
    const auto repeated = musdesk::analyzeRhythm(pcm, rate, 2);
    require(repeated.bass.values == rhythm.bass.values && repeated.beats == rhythm.beats,
        "Bass analysis must be deterministic");
    const auto pulse = rhythm.pulseAt(0.36);
    const auto strength = rhythm.beats.front().strength;
    require(rhythm.pulseAt(0.4, strength) > 0,
        "Beat gate must select onset strength before applying pulse decay");
    musdesk::AudioRhythm gated;
    gated.bass.durationSeconds = 2;
    gated.beats = {{0.2, 0.8f}, {0.4, 0.2f}};
    require(gated.pulseAt(0.3, 0.9f) == 0 && gated.pulseAt(0.3, 0.6f) > 0,
        "Gate must reject weak events without clipping the decay of accepted events");
    require(gated.pulseAt(0.45, 0.6f) == 0.8f * static_cast<float>(std::exp(-0.25 / 0.14)),
        "A weak onset must not interrupt the decay of a preceding strong beat");
    require(gated.pulseAt(0.3, std::numeric_limits<float>::quiet_NaN()) == 0, "Invalid gate must produce no pulse");
    (void)rhythm.pulseAt(1.8);
    require(rhythm.pulseAt(0.36) == pulse, "Seeking must not depend on previous pulse queries");
    require(std::all_of(rhythm.bass.values.begin(), rhythm.bass.values.end(), [](float value) {
        return std::isfinite(value) && value >= 0 && value <= 1;
    }), "Bass envelope must be finite and normalized");
    std::stop_source stop;
    stop.request_stop();
    rejected([&] { musdesk::analyzeRhythm(pcm, rate, 2, stop.get_token()); });
    rejected([&] { musdesk::analyzeRhythm({}, rate, 2); });
    rejected([&] { musdesk::analyzeRhythm(pcm, 0, 2); });
    rejected([&] { musdesk::analyzeRhythm(pcm, rate, 0); });
    rejected([&] { musdesk::analyzeRhythm(std::span(pcm).first(5), rate, 2); });
    std::cout << "Bass band, onset timing, pulse decay, sustained tone, silence and deterministic seeks passed.\n";
}
}

int main() {
    try {
        waveformTests();
        rhythmTests();
        constexpr int rate = 48000;
        std::vector<std::int16_t> samples(rate * 2 * 2, 0);
        require(musdesk::analyzeRms(samples, rate, 2).at(0.5) == 0, "Silence must have zero energy");
        for (int frame = rate / 2; frame < rate; ++frame) {
            samples[frame * 2] = 16384;
            samples[frame * 2 + 1] = -16384;
        }
        const auto envelope = musdesk::analyzeRms(samples, rate, 2);
        require(envelope.at(0.4) == 0 && envelope.at(0.7) > 0.95f, "RMS must retain opposite-phase stereo energy");
        require(envelope.at(0.52) > 0 && envelope.at(0.52) < envelope.at(0.7), "Attack must smooth the onset");
        require(envelope.at(1.1) > envelope.at(1.4) && envelope.at(1.4) > 0, "Release must decay gradually");
        require(envelope.at(-1) == 0 && envelope.at(2) == 0
            && envelope.at(std::numeric_limits<double>::quiet_NaN()) == 0, "Invalid and ended times must be silent");
        require(std::all_of(envelope.values.begin(), envelope.values.end(), [](float v) { return std::isfinite(v) && v >= 0 && v <= 1; }),
            "Envelope must be finite and normalized");
        const auto beforeSeek = envelope.at(0.63);
        (void)envelope.at(1.5);
        require(beforeSeek == envelope.at(0.63)
            && envelope.values == musdesk::analyzeRms(samples, rate, 2).values, "RMS must be repeatable across seeks and analyses");
        std::stop_source cancellation;
        cancellation.request_stop();
        bool cancelled = false;
        try { (void)musdesk::analyzeRms(samples, rate, 2, cancellation.get_token()); }
        catch (const std::exception&) { cancelled = true; }
        require(cancelled, "Analysis must honor cancellation");
        std::cout << "RMS stereo, silence, smoothing, normalization, cancellation and seek checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
