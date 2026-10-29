#include "musdesk/presets/PresetStore.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace musdesk {
namespace {
using Json = nlohmann::json;
constexpr std::size_t maxPresetBytes = 64 * 1024;

void fields(const Json& value, std::initializer_list<const char*> names) {
    if (!value.is_object() || value.size() != names.size())
        throw std::runtime_error("Preset object has missing or unknown fields.");
    for (auto name : names) {
        if (!value.contains(name)) throw std::runtime_error(std::string("Missing preset field: ") + name);
    }
}

double number(const Json& value, double minimum, double maximum, const char* name) {
    if (!value.is_number()) throw std::runtime_error(std::string("Preset field must be numeric: ") + name);
    const auto result = value.get<double>();
    if (!std::isfinite(result) || result < minimum || result > maximum)
        throw std::runtime_error(std::string("Preset value out of range: ") + name);
    return result;
}

struct TemporaryFile {
    std::string path;
    bool committed = false;
    ~TemporaryFile() { if (!committed) SDL_RemovePath(path.c_str()); }
};
}

VisualPreset parsePreset(std::string_view text) {
    if (text.empty() || text.size() > maxPresetBytes) throw std::runtime_error("Preset JSON must contain 1-65536 bytes.");
    try {
        std::vector<std::unordered_set<std::string>> keys;
        const auto json = Json::parse(text.begin(), text.end(), [&](int depth, Json::parse_event_t event, Json& value) {
            if (depth > 16) throw std::runtime_error("Preset JSON is nested too deeply.");
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
                throw std::runtime_error("Preset JSON contains a duplicate field.");
            if (event == Json::parse_event_t::object_end) keys.pop_back();
            return true;
        });
        if (!json.at("version").is_number_integer() || json.at("version") < 1 || json.at("version") > 6)
            throw std::runtime_error("Unsupported preset version; expected integer 1..6.");
        const bool legacy = json.at("version") == 1;
        if (legacy) fields(json, {"version", "name", "disc_rpm", "glow", "background"});
        else if (json.at("version") < 5) fields(json, {"version", "name", "disc_rpm", "glow", "background", "reactivity"});
        else fields(json, {"version", "name", "disc_rpm", "glow", "background", "reactivity", "effects"});
        if (!json.at("name").is_string()) throw std::runtime_error("Preset name must be a string.");
        const auto& glow = json.at("glow");
        const auto& background = json.at("background");
        fields(glow, {"color", "intensity", "pulse_strength"});
        fields(background, {"speed", "zoom", "brightness"});
        const auto& color = glow.at("color");
        if (!color.is_array() || color.size() != 3) throw std::runtime_error("Glow color must contain three RGB components.");
        VisualPreset preset;
        preset.spectrumEnabled = preset.wavesEnabled = true;
        preset.spectrumStrength = 0;
        preset.name = json.at("name").get<std::string>();
        preset.discRpm = number(json.at("disc_rpm"), -45, 45, "disc_rpm");
        for (int i = 0; i < 3; ++i) preset.glowColor[i] = static_cast<float>(number(color[i], 0, 1, "glow.color"));
        preset.glowIntensity = static_cast<float>(number(glow.at("intensity"), 0, 3, "glow.intensity"));
        preset.pulseStrength = static_cast<float>(number(glow.at("pulse_strength"), 0, 2, "glow.pulse_strength"));
        preset.backgroundSpeed = number(background.at("speed"), 0, 3, "background.speed");
        preset.backgroundZoom = static_cast<float>(number(background.at("zoom"), 1.1, 1.5, "background.zoom"));
        preset.backgroundBrightness = static_cast<float>(number(background.at("brightness"), 0, 1.5, "background.brightness"));
        if (legacy) {
            preset.subjectPulse = preset.backgroundReactivity = preset.ringReactivity = 0;
        } else {
            const auto& reactivity = json.at("reactivity");
            if (json.at("version") == 2) fields(reactivity, {"beat_threshold", "subject_pulse", "background", "ring"});
            else if (json.at("version") == 3) fields(reactivity, {"beat_threshold", "subject_pulse", "background", "ring", "spectrum"});
            else if (json.at("version") < 6) fields(reactivity, {"beat_threshold", "subject_pulse", "background", "ring", "spectrum", "spectrum_gain_db", "spectrum_balance"});
            else fields(reactivity, {"beat_level_db", "beat_band", "subject_pulse", "background", "ring", "spectrum", "spectrum_gain_db", "spectrum_balance"});
            if (json.at("version") < 6)
                preset.beatThreshold = static_cast<float>(number(reactivity.at("beat_threshold"), 0, 1, "reactivity.beat_threshold"));
            else {
                preset.beatThreshold = beatLevelFromDb(number(reactivity.at("beat_level_db"), -60, 0, "reactivity.beat_level_db"));
                const auto band = reactivity.at("beat_band").get<std::string>();
                if (band == "bass") preset.beatBand = BeatBand::Bass;
                else if (band == "drums") preset.beatBand = BeatBand::Drums;
                else if (band == "wide") preset.beatBand = BeatBand::Wide;
                else throw std::runtime_error("Beat band must be bass, drums or wide.");
            }
            preset.subjectPulse = static_cast<float>(number(reactivity.at("subject_pulse"), 0, 0.12, "reactivity.subject_pulse"));
            preset.backgroundReactivity = static_cast<float>(number(reactivity.at("background"), 0, 1, "reactivity.background"));
            preset.ringReactivity = static_cast<float>(number(reactivity.at("ring"), 0, 1, "reactivity.ring"));
            if (json.at("version") >= 3)
                preset.spectrumStrength = static_cast<float>(number(reactivity.at("spectrum"), 0, json.at("version") == 6 ? 4 : 1, "reactivity.spectrum"));
            if (json.at("version") >= 4) {
                preset.spectrumGainDb = static_cast<float>(number(reactivity.at("spectrum_gain_db"), -24, 24, "reactivity.spectrum_gain_db"));
                preset.spectrumBalance = static_cast<float>(number(reactivity.at("spectrum_balance"), -1, 1, "reactivity.spectrum_balance"));
            }
        }
        if (json.at("version") >= 5) {
            const auto& effects = json.at("effects");
            if (json.at("version") == 5) fields(effects, {"glow", "spectrum", "waves", "subject", "background", "camera", "camera_strength", "camera_threshold"});
            else fields(effects, {"glow", "spectrum", "waves", "subject", "background", "camera", "camera_strength"});
            const auto flag = [&](const char* name) {
                if (!effects.at(name).is_boolean()) throw std::runtime_error(std::string("Effect flag must be boolean: ") + name);
                return effects.at(name).get<bool>();
            };
            preset.glowEnabled = flag("glow");
            preset.spectrumEnabled = flag("spectrum");
            preset.wavesEnabled = flag("waves");
            preset.subjectEnabled = flag("subject");
            preset.backgroundEnabled = flag("background");
            preset.cameraEnabled = flag("camera");
            preset.cameraStrength = static_cast<float>(number(effects.at("camera_strength"), 0, 0.05f, "effects.camera_strength"));
            if (json.at("version") == 5) {
                const auto cameraGate = static_cast<float>(number(effects.at("camera_threshold"), 0, 1, "effects.camera_threshold"));
                if (preset.cameraEnabled) preset.beatThreshold = std::max(preset.beatThreshold,cameraGate);
            }
        }
        if (json.at("version") < 6 && (preset.name == "Soft Glow" || preset.name == "Beat Pulse" || preset.name == "Side Kick"))
            preset.spectrumEnabled = preset.wavesEnabled = false;
        validatePreset(preset);
        return preset;
    } catch (const Json::exception& error) {
        throw std::runtime_error(std::string("Invalid preset JSON: ") + error.what());
    }
}

std::string serializePreset(const VisualPreset& preset) {
    validatePreset(preset);
    try {
        const char* bands[]{"bass","drums","wide"};
        const Json json{{"version", 6}, {"name", preset.name}, {"disc_rpm", preset.discRpm},
            {"effects", {{"glow", preset.glowEnabled}, {"spectrum", preset.spectrumEnabled},
                {"waves", preset.wavesEnabled}, {"subject", preset.subjectEnabled}, {"background", preset.backgroundEnabled},
                {"camera", preset.cameraEnabled}, {"camera_strength", preset.cameraStrength}}},
            {"glow", {{"color", preset.glowColor}, {"intensity", preset.glowIntensity}, {"pulse_strength", preset.pulseStrength}}},
            {"background", {{"speed", preset.backgroundSpeed}, {"zoom", preset.backgroundZoom}, {"brightness", preset.backgroundBrightness}}},
            {"reactivity", {{"beat_level_db", beatLevelDb(preset.beatThreshold)}, {"beat_band", bands[static_cast<int>(preset.beatBand)]}, {"subject_pulse", preset.subjectPulse},
                {"background", preset.backgroundReactivity}, {"ring", preset.ringReactivity}, {"spectrum", preset.spectrumStrength},
                {"spectrum_gain_db", preset.spectrumGainDb}, {"spectrum_balance", preset.spectrumBalance}}}};
        return json.dump(2) + '\n';
    } catch (const Json::exception& error) {
        throw std::runtime_error(std::string("Cannot serialize preset: ") + error.what());
    }
}

VisualPreset loadPreset(const std::string& path) {
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(SDL_IOFromFile(path.c_str(), "rb"), SDL_CloseIO);
    if (!stream) throw std::runtime_error(std::string("Cannot open preset: ") + SDL_GetError());
    const auto length = SDL_GetIOSize(stream.get());
    if (length <= 0 || length > maxPresetBytes) throw std::runtime_error("Preset file must contain 1-65536 bytes.");
    std::string text(static_cast<std::size_t>(length), '\0');
    if (SDL_ReadIO(stream.get(), text.data(), text.size()) != text.size()) throw std::runtime_error("Cannot read complete preset file.");
    return parsePreset(text);
}

void savePreset(const std::string& path, const VisualPreset& preset) {
    const auto text = serializePreset(preset);
    static std::atomic<unsigned> counter{0};
    std::string temporary;
    TemporaryFile cleanup{{}, true};
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(nullptr, SDL_CloseIO);
    for (int attempt = 0; attempt < 8 && !stream; ++attempt) {
        temporary = path + ".tmp-" + std::to_string(SDL_GetTicksNS()) + "-" + std::to_string(counter++);
        stream.reset(SDL_IOFromFile(temporary.c_str(), "wbx"));
    }
    if (!stream) throw std::runtime_error(std::string("Cannot create preset file: ") + SDL_GetError());
    cleanup.path = temporary;
    cleanup.committed = false;
    if (SDL_WriteIO(stream.get(), text.data(), text.size()) != text.size() || !SDL_FlushIO(stream.get()))
        throw std::runtime_error(std::string("Cannot write preset: ") + SDL_GetError());
    if (!SDL_CloseIO(stream.release())) throw std::runtime_error(std::string("Cannot close preset: ") + SDL_GetError());
    if (!SDL_RenamePath(temporary.c_str(), path.c_str()))
        throw std::runtime_error(std::string("Cannot replace preset: ") + SDL_GetError());
    cleanup.committed = true;
}
}
