#include "musdesk/presets/PresetStore.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
template<class Action> void rejected(Action action, const char* reason = "") {
    try { action(); }
    catch (const std::exception& error) {
        require(std::string(error.what()).find(reason) != std::string::npos, "Unexpected preset error");
        return;
    }
    throw std::runtime_error("Invalid preset was accepted");
}
std::string replace(std::string text, const std::string& from, const std::string& to) {
    const auto position = text.find(from);
    require(position != std::string::npos, "Preset test mutation failed");
    text.replace(position, from.size(), to);
    return text;
}
std::string replaceNumber(std::string text, const std::string& key, const std::string& value) {
    const auto field = '"'+key+"\": ";
    const auto found = text.find(field);
    require(found != std::string::npos,"Preset numeric mutation failed");
    const auto begin = found+field.size();
    text.replace(begin,text.find_first_of(",\r\n}",begin)-begin,value);
    return text;
}
}

int main(int argc, char* argv[]) {
    try {
        const auto soft = musdesk::softGlowPreset();
        const auto beat = musdesk::beatPulsePreset();
        const auto kick = musdesk::sideKickPreset();
        const auto spectrum = musdesk::spectrumPulsePreset();
        require(!soft.spectrumEnabled && !soft.wavesEnabled && !beat.spectrumEnabled && !beat.wavesEnabled,
            "Glow presets must not enable line effects");
        require(spectrum.spectrumEnabled && !spectrum.glowEnabled,
            "Spectrum must have its own preset independent of glow");
        require(musdesk::parsePreset(musdesk::serializePreset(kick)) == kick,
            "Side Kick switches and camera settings must round trip");
        musdesk::validatePreset(soft);
        musdesk::validatePreset(beat);
        const auto json = musdesk::serializePreset(soft);
        const auto legacy = musdesk::parsePreset(R"({"version":1,"name":"Soft Glow","disc_rpm":8,
            "glow":{"color":[0.24,0.6,1],"intensity":0.8,"pulse_strength":0.6},
            "background":{"speed":0.6,"zoom":1.16,"brightness":0.88}})");
        auto legacyExpected = soft;
        legacyExpected.subjectPulse = legacyExpected.backgroundReactivity = legacyExpected.ringReactivity = 0;
        legacyExpected.spectrumStrength = 0;
        require(legacy == legacyExpected && musdesk::parsePreset(musdesk::serializePreset(legacy)) == legacy,
            "Version 1 presets must migrate without enabling new effects");
        const auto version2 = musdesk::parsePreset(R"({"version":2,"name":"Old reaction","disc_rpm":8,
            "glow":{"color":[0.24,0.6,1],"intensity":0.8,"pulse_strength":0.6},
            "background":{"speed":0.6,"zoom":1.16,"brightness":0.88},
            "reactivity":{"beat_threshold":0.55,"subject_pulse":0.05,"background":0.65,"ring":0.55}})");
        require(version2.spectrumStrength == 0 && version2.subjectPulse == soft.subjectPulse,
            "Version 2 presets must keep reactions while leaving FFT disabled");
        const auto version3 = musdesk::parsePreset(R"({"version":3,"name":"Old FFT","disc_rpm":8,
            "glow":{"color":[0.24,0.6,1],"intensity":0.8,"pulse_strength":0.6},
            "background":{"speed":0.6,"zoom":1.16,"brightness":0.88},
            "reactivity":{"beat_threshold":0.55,"subject_pulse":0.05,"background":0.65,"ring":0.55,"spectrum":0.65}})");
        require(version3.spectrumGainDb == 0 && version3.spectrumBalance == 0
            && version3.spectrumStrength == soft.spectrumStrength, "Version 3 spectra must retain neutral gain/balance");
        const auto version4 = musdesk::parsePreset(R"({"version":4,"name":"Old tuning","disc_rpm":8,
            "glow":{"color":[0.24,0.6,1],"intensity":0.8,"pulse_strength":0.6},
            "background":{"speed":0.6,"zoom":1.16,"brightness":0.88},
            "reactivity":{"beat_threshold":0.55,"subject_pulse":0.05,"background":0.65,"ring":0.55,
                "spectrum":0.65,"spectrum_gain_db":-5,"spectrum_balance":0.2}})");
        require(version4.glowEnabled && version4.spectrumEnabled && !version4.cameraEnabled
            && version4.spectrumGainDb == -5, "Version 4 must keep tuning without enabling camera kicks");
        const auto version5 = musdesk::parsePreset(R"({"version":5,"name":"Old kick","disc_rpm":8,
            "glow":{"color":[0.24,0.6,1],"intensity":0.8,"pulse_strength":0.6},
            "background":{"speed":0.6,"zoom":1.16,"brightness":0.88},
            "reactivity":{"beat_threshold":0.55,"subject_pulse":0.05,"background":0.65,"ring":0.55,
                "spectrum":0.65,"spectrum_gain_db":0,"spectrum_balance":0},
            "effects":{"glow":true,"spectrum":false,"waves":false,"subject":false,"background":true,
                "camera":true,"camera_strength":0.025,"camera_threshold":0.8}})");
        require(version5.beatThreshold == 0.8f && version5.beatBand == musdesk::BeatBand::Bass,
            "Version 5 must merge its two gates using the stricter threshold");
        const auto version6 = musdesk::parsePreset(R"({"version":6,"name":"Previous release","disc_rpm":8,
            "glow":{"color":[0.9,0.2,0.1],"intensity":0.8,"pulse_strength":0.6},
            "background":{"speed":0.6,"zoom":1.16,"brightness":0.88},
            "reactivity":{"beat_level_db":-27,"beat_band":"bass","subject_pulse":0.05,"background":0.65,
                "ring":0.55,"spectrum":3,"spectrum_gain_db":0,"spectrum_balance":0},
            "effects":{"glow":true,"spectrum":true,"waves":false,"subject":false,"background":true,
                "camera":false,"camera_strength":0.025}})");
        require(version6.spectrumColor == version6.glowColor && version6.spectrumPoints == 128
            && version6.glowWidth == 1 && version6.spectrumReleaseMs == 0
            && musdesk::parsePreset(musdesk::serializePreset(version6)) == version6,
            "Version 6 must migrate with matching colors and neutral new controls");
        require(musdesk::parsePreset(json) == soft && musdesk::parsePreset(musdesk::serializePreset(beat)) == beat,
            "Built-in preset round trip must preserve every value");
        if (argc == 2) {
            const auto examples = std::filesystem::u8path(argv[1]);
            require(musdesk::loadPreset(utf8(examples / "soft-glow.json")) == soft
                && musdesk::loadPreset(utf8(examples / "beat-pulse.json")) == beat
                && musdesk::loadPreset(utf8(examples / "side-kick.json")) == kick
                && musdesk::loadPreset(utf8(examples / "spectrum-pulse.json")) == spectrum, "Example JSON and built-in presets must agree");
        }
        auto custom = beat;
        custom.name = "Nocna plaża \"mix\"";
        custom.discRpm = -20;
        custom.backgroundSpeed = 0;
        custom.backgroundBrightness = 0.7f;
        custom.glowColor = {0, 1, 0.5f};
        custom.beatThreshold = 0.73f;
        custom.subjectPulse = 0.11f;
        custom.backgroundReactivity = 0.35f;
        custom.ringReactivity = 0.42f;
        custom.spectrumStrength = 0.37f;
        custom.spectrumGainDb = -6.5f;
        custom.spectrumBalance = 0.35f;
        custom.glowEnabled = custom.wavesEnabled = custom.backgroundEnabled = false;
        custom.subjectEnabled = custom.spectrumEnabled = false;
        custom.cameraEnabled = true;
        custom.cameraStrength = 0.05f;
        custom.beatBand = musdesk::BeatBand::Drums;
        custom.spectrumColor = {1,0.2f,0.4f};
        custom.spectrumPoints = 71;
        custom.spectrumSharpness = 0.32f;
        custom.spectrumRandomness = 0.42f;
        custom.spectrumSpeed = 2.4f;
        custom.spectrumLineWidth = 2.1f;
        custom.spectrumGap = 0.12f;
        custom.spectrumIntensity = 2.3f;
        custom.spectrumAttackMs = 120;
        custom.spectrumReleaseMs = 350;
        custom.glowWidth = 2;
        custom.glowSoftness = 0.4f;
        custom.glowReactivity = 2.1f;
        custom.glowBase = 0.2f;
        custom.glowAttackMs = 40;
        custom.glowReleaseMs = 750;
        auto reset = custom;
        musdesk::resetGlow(reset);
        require(reset.glowWidth == 1 && reset.glowBase == 1 && reset.glowReleaseMs == 0
            && reset.spectrumColor == custom.spectrumColor && reset.pulseStrength == custom.pulseStrength,
            "Glow reset must leave spectrum and shared response alone");
        reset = custom;
        musdesk::resetSpectrum(reset);
        require(reset.spectrumPoints == 128 && reset.spectrumReleaseMs == 0
            && reset.glowWidth == custom.glowWidth && reset.cameraEnabled == custom.cameraEnabled,
            "Spectrum reset must preserve the other effects");
        const auto folder = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures";
        std::filesystem::create_directories(folder);
        const auto path = utf8(folder / std::filesystem::path(u8"własny preset.json"));
        musdesk::savePreset(path, soft);
        musdesk::savePreset(path, custom);
        require(musdesk::loadPreset(path) == custom, "Overwrite, Unicode path and all custom values must round trip");
        auto invalid = custom;
        invalid.pulseStrength = std::numeric_limits<float>::quiet_NaN();
        rejected([&] { musdesk::savePreset(path, invalid); });
        require(musdesk::loadPreset(path) == custom, "Failed save must preserve the existing file");
        rejected([&] { musdesk::savePreset(utf8(folder), custom); });
        for (const auto& item : std::filesystem::directory_iterator(folder.parent_path())) {
            require(!item.path().filename().string().starts_with("test-fixtures.tmp-"), "Failed save must clean up its own temporary file");
        }
        rejected([&] { (void)musdesk::loadPreset(utf8(folder / "missing-preset.json")); });
        rejected([&] { (void)musdesk::parsePreset("{"); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 7", "\"version\": 8")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 7", "\"version\": 7.0")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"disc_rpm\": 8.0", "\"disc_rpm\": true")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"disc_rpm\": 8.0", "\"disc_rpm\": 46")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"name\": \"Soft Glow\"", "\"name\": \"\"")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 7", "\"unknown\": 0")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 7", "\"version\": 7, \"version\": 7")); }, "duplicate");
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"spectrum_gain_db\": 0.0", "\"spectrum_gain_db\": 25")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"camera\": false", "\"camera\": 0")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"camera_strength\": 0.025", "\"camera_strength\": 0.9")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"beat_band\": \"bass\"", "\"beat_band\": \"invalid\"")); });
        rejected([&] { (void)musdesk::parsePreset(replaceNumber(json,"beat_level_db","1")); });
        rejected([&] { (void)musdesk::parsePreset(replaceNumber(json,"beat_level_db","-61")); });
        rejected([&] { (void)musdesk::parsePreset(replaceNumber(json,"beat_level_db","true")); });
        rejected([&] { (void)musdesk::parsePreset(replaceNumber(json,"spectrum","4.1")); });
        require(musdesk::parsePreset(replaceNumber(json,"width","0.2")).glowWidth == 0.2f
            && musdesk::parsePreset(replaceNumber(json,"gap","0.015")).spectrumGap == 0.015f,
            "Decimal lower bounds must remain valid JSON inputs");
        for (const auto& mutation : {std::pair{"points","31"}, {"points","128.5"}, {"points","true"},
            {"sharpness","1.1"},{"randomness","-1"},{"line_width","0"},{"gap","0"},
            {"speed","5"},{"attack_ms","251"},{"release_ms","1001"},{"softness","0"},
            {"width","4"},{"base","-1"},{"reactivity","4"}}) {
            rejected([&] { (void)musdesk::parsePreset(replaceNumber(json,mutation.first,mutation.second)); });
        }
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"subject_pulse\": 0.05", "\"subject_pulse\": -0.05")); });
        rejected([&] { (void)musdesk::parsePreset(std::string(65537, ' ')); });
        rejected([&] { (void)musdesk::parsePreset(std::string(20, '[') + "0" + std::string(20, ']')); }, "deeply");
        std::cout << "Preset examples, custom round trip, Unicode paths, overwrite, atomic failure and JSON validation checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
