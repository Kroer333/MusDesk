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
}

int main(int argc, char* argv[]) {
    try {
        const auto soft = musdesk::softGlowPreset();
        const auto beat = musdesk::beatPulsePreset();
        musdesk::validatePreset(soft);
        musdesk::validatePreset(beat);
        const auto json = musdesk::serializePreset(soft);
        require(musdesk::parsePreset(json) == soft && musdesk::parsePreset(musdesk::serializePreset(beat)) == beat,
            "Built-in preset round trip must preserve every value");
        if (argc == 2) {
            const auto examples = std::filesystem::u8path(argv[1]);
            require(musdesk::loadPreset(utf8(examples / "soft-glow.json")) == soft
                && musdesk::loadPreset(utf8(examples / "beat-pulse.json")) == beat, "Example JSON and built-in presets must agree");
        }
        auto custom = beat;
        custom.name = "Nocna plaża \"mix\"";
        custom.discRpm = -20;
        custom.backgroundSpeed = 0;
        custom.backgroundBrightness = 0.7f;
        custom.glowColor = {0, 1, 0.5f};
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
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 1", "\"version\": 2")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 1", "\"version\": 1.0")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"disc_rpm\": 8.0", "\"disc_rpm\": true")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"disc_rpm\": 8.0", "\"disc_rpm\": 46")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"name\": \"Soft Glow\"", "\"name\": \"\"")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 1", "\"unknown\": 0")); });
        rejected([&] { (void)musdesk::parsePreset(replace(json, "\"version\": 1", "\"version\": 1, \"version\": 1")); }, "duplicate");
        rejected([&] { (void)musdesk::parsePreset(std::string(65537, ' ')); });
        rejected([&] { (void)musdesk::parsePreset(std::string(20, '[') + "0" + std::string(20, ']')); }, "deeply");
        std::cout << "Preset examples, custom round trip, Unicode paths, overwrite, atomic failure and JSON validation checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
