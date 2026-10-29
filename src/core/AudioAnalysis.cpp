#include "musdesk/core/AudioAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <array>
#include <numbers>
#include <complex>
#include <stdexcept>

namespace musdesk {
SpectrumFrame spectrumForDisplay(const SpectrumFrame& spectrum, float gainDb, float balance) {
    gainDb = std::isfinite(gainDb) ? std::clamp(gainDb, -24.0f, 24.0f) : 0;
    balance = std::isfinite(balance) ? std::clamp(balance, -1.0f, 1.0f) : 0;
    SpectrumFrame result{};
    for (std::size_t band = 0; band < result.size(); ++band) {
        if (!std::isfinite(spectrum[band]) || spectrum[band] <= 0) continue;
        const float position = 2.0f * band / (result.size() - 1) - 1;
        result[band] = std::clamp(std::min(spectrum[band], 1.0f) + (gainDb + 12 * balance * position) / 60, 0.0f, 1.0f);
    }
    return result;
}

SpectrumFrame AudioSpectrum::at(double seconds) const {
    SpectrumFrame result{};
    if (frames.empty() || !std::isfinite(seconds) || seconds < 0 || seconds >= durationSeconds
        || !std::isfinite(stepSeconds) || stepSeconds <= 0) return result;
    const double position = seconds / stepSeconds;
    const auto index = static_cast<std::size_t>(std::min(position, static_cast<double>(frames.size() - 1)));
    const auto next = std::min(index + 1, frames.size() - 1);
    const auto fraction = static_cast<float>(std::clamp(position - index, 0.0, 1.0));
    for (std::size_t band = 0; band < result.size(); ++band)
        result[band] = std::lerp(frames[index][band], frames[next][band], fraction);
    return result;
}

AudioSpectrum analyzeSpectrum(std::span<const std::int16_t> samples, int sampleRate, int channels,
    std::stop_token cancellation) {
    if (sampleRate < 1000 || sampleRate > 384000 || channels < 1 || channels > 8
        || samples.empty() || samples.size() % channels != 0)
        throw std::runtime_error("Invalid PCM audio for spectrum analysis.");
    const auto frameCount = samples.size() / channels;
    if (static_cast<double>(frameCount) / sampleRate > 1200)
        throw std::runtime_error("Spectrum analysis supports tracks up to 20 minutes.");
    constexpr std::size_t size = 2048;
    using Complex = std::complex<float>;
    std::array<Complex, size / 2> roots;
    std::array<float, size> window;
    std::array<std::size_t, size> reverse{};
    for (std::size_t i = 0; i < size; ++i) {
        window[i] = static_cast<float>(0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / size));
        for (std::size_t bits = i, count = size; count > 1; count >>= 1, bits >>= 1)
            reverse[i] = (reverse[i] << 1) | (bits & 1);
        if (i < roots.size()) roots[i] = std::polar(1.0f, static_cast<float>(-2 * std::numbers::pi * i / size));
    }
    const auto hop = static_cast<std::size_t>(sampleRate / 50);
    AudioSpectrum result;
    result.stepSeconds = static_cast<double>(hop) / sampleRate;
    result.durationSeconds = static_cast<double>(frameCount) / sampleRate;
    result.frames.reserve((frameCount + hop - 1) / hop + 1);
    SpectrumFrame smoothed{};
    const auto attack = static_cast<float>(1 - std::exp(-result.stepSeconds / 0.010));
    const auto release = static_cast<float>(1 - std::exp(-result.stepSeconds / 0.060));
    for (std::size_t center = 0; center < frameCount + hop; center += hop) {
        if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
        std::array<float, size / 2 + 1> power{};
        for (int channel = 0; channel < channels; ++channel) {
            std::array<Complex, size> fft{};
            double mean = 0;
            std::size_t valid = 0;
            for (std::size_t i = 0; i < size; ++i) {
                const auto frame = static_cast<std::int64_t>(center) + static_cast<std::int64_t>(i) - static_cast<std::int64_t>(size / 2);
                if (frame >= 0 && static_cast<std::size_t>(frame) < frameCount) {
                    mean += samples[frame * channels + channel] / 32768.0;
                    ++valid;
                }
            }
            if (valid) mean /= valid;
            for (std::size_t i = 0; i < size; ++i) {
                const auto frame = static_cast<std::int64_t>(center) + static_cast<std::int64_t>(i) - static_cast<std::int64_t>(size / 2);
                if (frame >= 0 && static_cast<std::size_t>(frame) < frameCount)
                    fft[reverse[i]] = static_cast<float>(samples[frame * channels + channel] / 32768.0 - mean) * window[i];
            }
            for (std::size_t length = 2; length <= size; length *= 2) {
                for (std::size_t base = 0; base < size; base += length) {
                    for (std::size_t i = 0; i < length / 2; ++i) {
                        const auto even = fft[base + i];
                        const auto odd = fft[base + i + length / 2] * roots[i * size / length];
                        fft[base + i] = even + odd;
                        fft[base + i + length / 2] = even - odd;
                    }
                }
            }
            for (std::size_t bin = 1; bin < power.size(); ++bin) power[bin] += std::norm(fft[bin]) / channels;
        }
        const double maximumHz = std::min(12000.0, sampleRate / 2.0);
        for (std::size_t band = 0; band < smoothed.size(); ++band) {
            const auto lower = 40 * std::pow(maximumHz / 40, static_cast<double>(band) / smoothed.size());
            const auto upper = 40 * std::pow(maximumHz / 40, static_cast<double>(band + 1) / smoothed.size());
            const double binPosition = std::sqrt(lower * upper) * size / sampleRate;
            const auto left = std::clamp<std::size_t>(static_cast<std::size_t>(binPosition), 1, size / 2 - 1);
            float peak = std::lerp(power[left], power[left + 1], static_cast<float>(std::clamp(binPosition - left, 0.0, 1.0)));
            for (auto bin = static_cast<std::size_t>(std::ceil(lower * size / sampleRate));
                bin < std::min<std::size_t>(power.size(), static_cast<std::size_t>(std::ceil(upper * size / sampleRate))); ++bin)
                peak = std::max(peak, power[bin]);
            const float magnitude = 4 * std::sqrt(peak) / size;
            const float target = std::clamp((20 * std::log10(std::max(magnitude, 1e-6f)) + 60) / 60, 0.0f, 1.0f);
            smoothed[band] += (target - smoothed[band]) * (target > smoothed[band] ? attack : release);
        }
        result.frames.push_back(smoothed);
    }
    return result;
}

namespace {
struct Biquad {
    double b0, b1, b2, a1, a2;
    double z1 = 0, z2 = 0;
    Biquad(double cutoff, int rate, bool highPass) {
        const double angle = 2 * std::numbers::pi * cutoff / rate;
        const double cosine = std::cos(angle);
        const double alpha = std::sin(angle) / std::sqrt(2.0);
        const double a0 = 1 + alpha;
        b0 = (highPass ? 1 + cosine : 1 - cosine) / (2 * a0);
        b1 = (highPass ? -(1 + cosine) : 1 - cosine) / a0;
        b2 = b0;
        a1 = -2 * cosine / a0;
        a2 = (1 - alpha) / a0;
    }
    double process(double input) {
        const double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        if (std::abs(z1) < 1e-20) z1 = 0;
        if (std::abs(z2) < 1e-20) z2 = 0;
        return output;
    }
};
}

float beatLevelFromDb(double db) {
    return std::isfinite(db) ? static_cast<float>(std::clamp((db+60)/60,0.0,1.0)) : 0;
}
double beatLevelDb(float level) { return std::clamp<double>(level,0,1)*60-60; }

const std::vector<BeatEvent>& AudioRhythm::events(BeatBand band) const {
    if (band == BeatBand::Drums) return otherBands[0];
    if (band == BeatBand::Wide) return otherBands[1];
    return beats;
}

float AudioRhythm::pulseAt(double timeSeconds, float minimumStrength, BeatBand band) const {
    if (!std::isfinite(timeSeconds) || timeSeconds < 0 || timeSeconds >= bass.durationSeconds
        || !std::isfinite(minimumStrength)) return 0;
    minimumStrength = std::clamp(minimumStrength, 0.0f, 1.0f);
    const auto& selected = events(band);
    auto next = std::upper_bound(selected.begin(), selected.end(), timeSeconds,
        [](double time, const BeatEvent& beat) { return time < beat.timeSeconds; });
    while (next != selected.begin()) {
        const auto& beat = *--next;
        if (beat.strength >= minimumStrength)
            return beat.strength * static_cast<float>(std::exp(-(timeSeconds - beat.timeSeconds) / 0.055));
    }
    return 0;
}

float AudioRhythm::kickAt(double timeSeconds, float minimumStrength, BeatBand band) const {
    if (!std::isfinite(timeSeconds) || timeSeconds < 0 || timeSeconds >= bass.durationSeconds
        || !std::isfinite(minimumStrength)) return 0;
    const auto& selected = events(band);
    const auto end = std::upper_bound(selected.begin(), selected.end(), timeSeconds,
        [](double time, const BeatEvent& beat) { return time < beat.timeSeconds; });
    const BeatEvent* latest = nullptr;
    std::size_t accepted = 0;
    for (auto it = selected.begin(); it != end; ++it) {
        if (it->strength >= std::clamp(minimumStrength, 0.0f, 1.0f)) {
            latest = &*it;
            ++accepted;
        }
    }
    if (!latest) return 0;
    return (accepted % 2 ? 1 : -1) * latest->strength
        * static_cast<float>(std::exp(-(timeSeconds - latest->timeSeconds) / 0.045));
}

AudioRhythm analyzeRhythm(std::span<const std::int16_t> samples, int sampleRate, int channels,
    std::stop_token cancellation) {
    if (sampleRate < 1000 || sampleRate > 384000 || channels < 1 || channels > 8
        || samples.empty() || samples.size() % channels != 0)
        throw std::runtime_error("Invalid PCM audio for bass analysis.");
    const auto frames = samples.size()/channels;
    if (static_cast<double>(frames)/sampleRate > 1200)
        throw std::runtime_error("Bass analysis supports tracks up to 20 minutes.");
    const auto window = static_cast<std::size_t>(sampleRate/200);
    AudioRhythm result;
    result.bass.stepSeconds = static_cast<double>(window)/sampleRate;
    result.bass.durationSeconds = static_cast<double>(frames)/sampleRate;
    std::array<std::vector<std::array<Biquad,4>>,3> filters;
    const double high = std::min(2500.0,sampleRate*0.45);
    for (int channel = 0; channel < channels; ++channel) {
        filters[0].push_back({Biquad(35,sampleRate,true),Biquad(35,sampleRate,true),Biquad(180,sampleRate,false),Biquad(180,sampleRate,false)});
        filters[1].push_back({Biquad(180,sampleRate,true),Biquad(180,sampleRate,true),Biquad(high,sampleRate,false),Biquad(high,sampleRate,false)});
        filters[2].push_back({Biquad(35,sampleRate,true),Biquad(35,sampleRate,true),Biquad(high,sampleRate,false),Biquad(high,sampleRate,false)});
    }
    std::array<std::vector<float>,3> energy;
    for (auto& band : energy) band.reserve((frames+window-1)/window);
    for (std::size_t begin = 0; begin < frames; begin += window) {
        if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
        const auto end = std::min(begin+window,frames);
        std::array<double,3> sum{};
        for (auto frame = begin; frame < end; ++frame) for (int channel = 0; channel < channels; ++channel) {
            const double input = samples[frame*channels+channel]/32768.0;
            for (std::size_t band = 0; band < filters.size(); ++band) {
                auto& filter = filters[band][channel];
                double value = input;
                for (auto& stage : filter) value = stage.process(value);
                sum[band] += value*value;
            }
        }
        for (std::size_t band = 0; band < energy.size(); ++band)
            energy[band].push_back(static_cast<float>(sum[band]/((end-begin)*channels)));
    }
    const float peak = std::max(0.03f,std::sqrt(*std::max_element(energy[0].begin(),energy[0].end())));
    const float attack = static_cast<float>(1-std::exp(-result.bass.stepSeconds/0.015));
    const float release = static_cast<float>(1-std::exp(-result.bass.stepSeconds/0.10));
    float smoothed = 0;
    result.bass.values.push_back(0);
    for (auto squared : energy[0]) {
        const float value = std::sqrt(squared);
        const float target = value < 0.0001f ? 0 : std::clamp(value/peak,0.0f,1.0f);
        smoothed += (target-smoothed)*(target > smoothed ? attack : release);
        result.bass.values.push_back(smoothed);
    }
    for (std::size_t band = 0; band < energy.size(); ++band) {
        auto& events = band == 0 ? result.beats : result.otherBands[band-1];
        std::vector<float> level,novelty;
        level.reserve(energy[band].size());
        novelty.reserve(energy[band].size());
        double rolling = 0;
        for (std::size_t i = 0; i < energy[band].size(); ++i) {
            if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
            rolling += energy[band][i];
            if (i >= 4) rolling -= energy[band][i-4];
            level.push_back(static_cast<float>(std::sqrt(std::max(0.0,rolling)/4)));
            novelty.push_back(std::max(0.0f,level[i]-(i >= 4 ? level[i-4] : 0)));
        }
        float lastAttack = 0, precedingBed = 0;
        for (std::size_t i = 1; i+1 < level.size(); ++i) {
            if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
            const float rise = novelty[i];
            if (rise < novelty[i-1] || rise <= novelty[i+1]
                || rise < std::max(0.0015f,level[i]*0.18f) || level[i] < 0.003f) continue;
            const float shoulders = ((i >= 2 ? novelty[i-2] : 0)
                +(i+2 < novelty.size() ? novelty[i+2] : 0))*0.5f;
            if (rise < shoulders*1.05f) continue;
            const auto last = std::min(i+5,level.size());
            const auto peakAt = static_cast<std::size_t>(std::max_element(level.begin()+i,level.begin()+last)-level.begin());
            if (peakAt+1 < level.size() && level[peakAt+1] > level[peakAt]*1.03f) continue;
            auto troughAt = peakAt;
            while (troughAt > 0 && peakAt-troughAt < 30 && level[troughAt-1] <= level[troughAt]+0.00001f) --troughAt;
            const float height = level[peakAt]-level[troughAt];
            if (height < std::max(0.0015f,level[peakAt]*0.18f)) continue;
            auto start = troughAt, upper = troughAt;
            while (start < peakAt && level[start] < level[troughAt]+height*0.2f) ++start;
            while (upper < peakAt && level[upper] < level[troughAt]+height*0.8f) ++upper;
            if ((upper-start)*result.bass.stepSeconds > 0.04) continue;
            const double time = start*result.bass.stepSeconds;
            const float amplitude = *std::max_element(level.begin()+i,level.begin()+last);
            const float strength = beatLevelFromDb(20*std::log10(std::max(amplitude,1e-6f)));
            const float bed = start >= 4 ? level[start-4] : 0;
            const bool replacing = !events.empty() && time-events.back().timeSeconds < 0.04;
            if (!replacing && !events.empty() && time-events.back().timeSeconds < 0.10
                && amplitude <= precedingBed*1.5f) continue;
            if (replacing) {
                if (rise <= lastAttack) continue;
                events.pop_back();
            }
            if (time < result.bass.durationSeconds) {
                events.push_back({time,strength});
                lastAttack = rise;
                precedingBed = replacing ? std::min(precedingBed,bed) : bed;
            }
        }
    }
    return result;
}

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
