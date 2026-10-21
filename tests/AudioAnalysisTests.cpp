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
}

int main() {
    try {
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
