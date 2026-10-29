#include "musdesk/projects/ProjectStore.hpp"
#include "musdesk/projects/ProjectCollector.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

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
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(file)), {}};
}
void collectionTests() {
    namespace fs = std::filesystem;
    const auto base = fs::u8path(SDL_GetBasePath()) / "test-fixtures"
        / ("collection-" + std::to_string(SDL_GetTicksNS()));
    const auto source = base / fs::path(u8"źródła");
    const auto destination = base / fs::path(u8"projekty z mediami");
    fs::create_directories(source / "one");
    fs::create_directories(source / "two");
    fs::create_directories(destination / "MusDesk-project");
    { std::ofstream(destination / "MusDesk-project" / "keep.txt") << "Existing project"; }
    const std::string audioBytes("audio\0fixture", 13);
    const std::string artworkBytes("artwork\0fixture", 15);
    { std::ofstream(source / "one" / "same.bin", std::ios::binary) << audioBytes; }
    { std::ofstream(source / "two" / "same.bin", std::ios::binary) << artworkBytes; }
    musdesk::ProjectDocument project;
    project.composition.title = "Collected composition";
    project.composition.preset = musdesk::beatPulsePreset();
    project.previewTimeSeconds = 12.25;
    project.audioPath = utf8(source / "one" / "same.bin");
    project.artworkPath = utf8(source / "two" / "same.bin");
    musdesk::saveProject(utf8(source / "original.musdesk"), project);
    const auto originalDocument = read(source / "original.musdesk");
    const auto original = project;
    const auto collectedPath = fs::u8path(musdesk::collectProject(utf8(destination), project));
    require(project == original, "Collection modified the caller's document");
    require(read(source / "original.musdesk") == originalDocument, "Collection changed the original project");
    require(read(source / "one" / "same.bin") == audioBytes
        && read(source / "two" / "same.bin") == artworkBytes, "Collection changed source media");
    require(read(destination / "MusDesk-project" / "keep.txt") == "Existing project", "Collection overwrote an existing folder");
    const auto stored = musdesk::parseProject(read(collectedPath));
    require(fs::u8path(stored.audioPath).is_relative() && fs::u8path(stored.artworkPath).is_relative(),
        "Collected paths must be relative");
    require(stored.audioPath != stored.artworkPath, "Same-name media collided");
#ifdef _WIN32
    const auto countBeforeFailure = std::distance(fs::directory_iterator(destination), fs::directory_iterator{});
    struct FileLock {
        HANDLE handle;
        ~FileLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    };
    {
        FileLock locked{CreateFileW(fs::u8path(project.artworkPath).c_str(), GENERIC_READ, 0,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        require(locked.handle != INVALID_HANDLE_VALUE, "Cannot lock collection fixture");
        rejected([&] { musdesk::collectProject(utf8(destination), project); });
        require(std::distance(fs::directory_iterator(destination), fs::directory_iterator{}) == countBeforeFailure,
            "Copy failure left partially collected media or damaged another collection");
    }
#endif
    const auto moved = base / "moved";
    fs::rename(collectedPath.parent_path(), moved);
    fs::rename(source, base / "unavailable-originals");
    const auto loaded = musdesk::loadProject(utf8(moved / "project.musdesk"));
    require(read(fs::u8path(loaded.audioPath)) == audioBytes
        && read(fs::u8path(loaded.artworkPath)) == artworkBytes, "Moved collection still depends on original media");
    auto expected = original;
    expected.audioPath = loaded.audioPath;
    expected.artworkPath = loaded.artworkPath;
    require(loaded == expected, "Collected project lost settings or preview position");
    const auto before = std::distance(fs::directory_iterator(destination), fs::directory_iterator{});
    rejected([&] { musdesk::collectProject(utf8(destination), original); });
    require(std::distance(fs::directory_iterator(destination), fs::directory_iterator{}) == before,
        "Missing media left a partial collection");
    std::stop_source cancellation;
    cancellation.request_stop();
    rejected([&] { musdesk::collectProject(utf8(destination), {}, cancellation.get_token()); });
    require(std::distance(fs::directory_iterator(destination), fs::directory_iterator{}) == before,
        "Cancelled collection left files");
    rejected([&] { musdesk::collectProject(utf8(destination / "absent"), {}); });
    rejected([&] { musdesk::collectProject(utf8(moved / "project.musdesk"), {}); });
    const auto empty = musdesk::collectProject(utf8(destination), {});
    require(musdesk::loadProject(empty) == musdesk::ProjectDocument{}, "Collection without media failed");
    std::cout << "Collection: portable relocation, binary copies, Unicode, collisions, missing media and cancellation passed.\n";
}
}
int main(int, char*[]) {
    try {
        musdesk::ProjectDocument project;
        project.composition.title = "Księżycowa plaża";
        project.composition.artist = "Zażółć gęślą jaźń";
        project.composition.preset = musdesk::beatPulsePreset();
        project.composition.preset.discRpm = -12;
        project.composition.preset.beatThreshold = 0.68f;
        project.composition.preset.subjectPulse = 0.10f;
        project.composition.preset.glowEnabled = false;
        project.composition.preset.cameraEnabled = true;
        project.composition.preset.cameraThreshold = 0.3f;
        project.composition.backgroundFraming = {1.8f,0.25f,0.7f};
        project.composition.labelFraming = {2,0.8f,0.2f};
        project.composition.backgroundSoftness = 0.35f;
        const auto legacy = musdesk::parseProject(R"({"version":1,"format":"9:16","title":"Legacy",
            "artist":"Artist","duration_seconds":252,"preview_time_seconds":4,"audio":"","artwork":"",
            "preset":{"version":1,"name":"Old","disc_rpm":8,
                "glow":{"color":[0.24,0.6,1],"intensity":0.8,"pulse_strength":0.6},
                "background":{"speed":0.6,"zoom":1.16,"brightness":0.88}}})");
        require(legacy.composition.preset.subjectPulse == 0 && legacy.composition.preset.backgroundReactivity == 0
            && legacy.composition.preset.ringReactivity == 0 && legacy.previewTimeSeconds == 4,
            "Old projects must preserve their visual settings when upgrading the embedded preset");
        require(musdesk::parseProject(musdesk::serializeProject(legacy)) == legacy, "Migrated project must round trip");
        const auto version2 = musdesk::parseProject(R"({"version":2,"format":"16:9","short_edge":720,
            "title":"Old landscape","artist":"Artist","duration_seconds":12,"preview_time_seconds":4,
            "audio":"","artwork":"","preset":{"version":1,"name":"Old","disc_rpm":8,
            "glow":{"color":[0.24,0.6,1],"intensity":0.8,"pulse_strength":0.6},
            "background":{"speed":0.6,"zoom":1.16,"brightness":0.88}}})");
        require(version2.composition.backgroundFraming == musdesk::ImageFraming{}
            && version2.composition.labelFraming == musdesk::ImageFraming{} && version2.composition.backgroundSoftness == 1,
            "Version 2 projects must preserve centered crops and previous softness");
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
        rejected([&] { (void)musdesk::parseProject(replace(json, "\n  \"version\": 3", "\n  \"version\": 4")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\n  \"version\": 3", "\n  \"version\": 3.0")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\n  \"version\": 3", "\n  \"version\": 3, \"version\": 3")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"focus_x\": 0.25", "\"focus_x\": 1.00000001")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"focus_x\": 0.25", "\"focus_x\": true")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"format\": \"9:16\"", "\"format\": \"5:4\"")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"short_edge\": 1080", "\"short_edge\": 721")); });
        rejected([&] { (void)musdesk::parseProject(replace(json, "\"short_edge\": 1080", "\"short_edge\": 4294968376")); });
        for (auto format : {musdesk::CompositionFormat::Portrait, musdesk::CompositionFormat::Landscape,
            musdesk::CompositionFormat::Square, musdesk::CompositionFormat::PortraitThreeFour, musdesk::CompositionFormat::LandscapeFourThree}) {
            for (int edge : {720, 1080}) {
                auto variant = project;
                variant.composition.format = format;
                variant.composition.shortEdge = edge;
                require(musdesk::parseProject(musdesk::serializeProject(variant)) == variant,
                    "Each layout/resolution must survive project round trip");
            }
        }
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
        collectionTests();
        std::cout << "Project settings, relative paths, relocation, missing media, safe overwrite and validation passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
