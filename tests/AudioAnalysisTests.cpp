#include "musdesk/core/AudioAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void rejected(Action action) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Invalid waveform input was accepted");
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
}

int main() {
    try {
        waveformTests();
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
