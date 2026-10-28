#include "musdesk/projects/ProjectStore.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
template<class Action> void rejected(Action action) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Invalid project was accepted");
}
std::string replace(std::string text, const std::string& old, const std::string& value) {
    const auto position = text.find(old);
    require(position != std::string::npos, "Test JSON fragment not found");
    text.replace(position, old.size(), value);
    return text;
}
}
int main(int, char*[]) {
    try {
        musdesk::ProjectDocument project;
        project.composition.title = "Księżycowa plaża";
        project.composition.artist = "Zażółć gęślą jaźń";
        project.composition.preset = musdesk::beatPulsePreset();
        project.composition.preset.discRpm = -12;
        project.previewTimeSeconds = 42.25;
        const auto json = musdesk::serializeProject(project);
        require(musdesk::parseProject(json) == project, "Complete settings/Unicode/preset round trip failed");
        require(musdesk::isProjectPath("SET.MUSDESK") && !musdesk::isProjectPath("set.json"), "Project extension detection failed");
        const auto base = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures" / "projects";
        const auto original = base / "original";
        std::filesystem::create_directories(original / "media");
        project.audioPath = utf8(original / "media" / std::filesystem::path(u8"muzyka.wav"));
        project.artworkPath = utf8(original / "media" / std::filesystem::path(u8"okładka.png"));
        const auto path = original / std::filesystem::path(u8"własny projekt.musdesk");
        musdesk::saveProject(utf8(path), project);
        require(musdesk::loadProject(utf8(path)) == project, "Absolute media paths must resolve after save/load");
        std::ifstream file(path);
        const std::string stored((std::istreambuf_iterator<char>(file)), {});
        file.close();
        require(stored.find("media/") != std::string::npos || stored.find("media\\\\") != std::string::npos,
            "Media paths should be relative to the project");
        require(stored.find("original") == std::string::npos, "Relative project must not embed its original folder");
        const auto moved = base / "moved";
        std::filesystem::create_directories(moved);
        std::filesystem::copy_file(path, moved / "project.musdesk", std::filesystem::copy_options::overwrite_existing);
        const auto relocated = musdesk::loadProject(utf8(moved / "project.musdesk"));
        require(relocated.audioPath == utf8(moved / "media" / "muzyka.wav"), "Moving a project folder must relocate relative media paths");
        require(!std::filesystem::exists(std::filesystem::u8path(relocated.audioPath)), "Missing-media fixture must actually be missing");
        musdesk::saveProject(utf8(moved / "save-as.musdesk"), project);
        require(musdesk::loadProject(utf8(moved / "save-as.musdesk")) == project,
            "Save As must rebase media paths without changing their resolved destinations");
        project.composition.title = "Changed";
        musdesk::saveProject(utf8(path), project);
        require(musdesk::loadProject(utf8(path)) == project, "Project overwrite failed");
        auto invalid = project;
        invalid.composition.preset.pulseStrength = std::numeric_limits<float>::quiet_NaN();
        rejected([&] { musdesk::saveProject(utf8(path), invalid); });
        require(musdesk::loadProject(utf8(path)) == project, "Invalid save destroyed the old project");
        rejected([&] { musdesk::saveProject(utf8(original / ("missing-" + std::to_string(SDL_GetTicksNS())) / "project.musdesk"), project); });
        std::filesystem::create_directories(original / "directory.musdesk");
        rejected([&] { musdesk::saveProject(utf8(original / "directory.musdesk"), project); });
        for (const auto& item : std::filesystem::directory_iterator(original))
            require(utf8(item.path().filename()).find(".tmp-") == std::string::npos, "Failed save left a temporary file");
        auto self = project;
        self.audioPath = utf8(path);
        rejected([&] { musdesk::saveProject(utf8(path), self); });
        require(musdesk::loadProject(utf8(path)) == project, "Media-overwrite protection destroyed the project");
        rejected([&] { (void)musdesk::parseProject("{"); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"version\": 1", "\"version\": 2")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"version\": 1", "\"version\": 1.0")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"version\": 1", "\"version\": 1, \"version\": 1")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"format\": \"9:16\"", "\"format\": \"4:3\"")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"duration_seconds\": 252.0", "\"duration_seconds\": true")); });
        rejected([&] { (void)musdesk::parseProject(std::string(262145, ' ')); });
        rejected([&] { (void)musdesk::parseProject(std::string(30, '[') + "0" + std::string(30, ']')); });
        for (const auto& title : {std::string(128, 'a'), std::string("bad\0name", 8)}) {
            invalid = project;
            invalid.composition.title = title;
            rejected([&] { (void)musdesk::serializeProject(invalid); });
        }
        invalid = project;
        invalid.audioPath = "https://example.invalid/audio.mp3";
        rejected([&] { (void)musdesk::serializeProject(invalid); });
        invalid = project;
        invalid.previewTimeSeconds = 2000;
        rejected([&] { (void)musdesk::serializeProject(invalid); });
        std::cout << "Project settings, relative paths, relocation, missing media, safe overwrite and validation passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
