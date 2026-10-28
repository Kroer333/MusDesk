#include "musdesk/projects/ProjectStore.hpp"
#include "musdesk/presets/PresetStore.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace musdesk {
namespace {
using Json = nlohmann::json;
constexpr std::size_t maxBytes = 256 * 1024;

std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
void text(const std::string& value, std::size_t limit, const char* name) {
    if (value.size() > limit) throw std::runtime_error(std::string("Project text is too long: ") + name);
    for (unsigned char c : value)
        if (c < 32 || c == 127) throw std::runtime_error(std::string("Control characters in project field: ") + name);
}
void validate(const ProjectDocument& project) {
    outputSize(project.composition.format, project.composition.shortEdge);
    validatePreset(project.composition.preset);
    validateImageFraming(project.composition.backgroundFraming);
    validateImageFraming(project.composition.labelFraming);
    if (!std::isfinite(project.composition.backgroundSoftness) || project.composition.backgroundSoftness < 0
        || project.composition.backgroundSoftness > 1) throw std::runtime_error("Background softness must be 0..1.");
    text(project.composition.title, 127, "title");
    text(project.composition.artist, 95, "artist");
    for (const auto& path : {project.audioPath, project.artworkPath}) {
        text(path, 32768, "media path");
        if (path.find("://") != std::string::npos)
            throw std::runtime_error("Project media must reference local files, not URLs.");
        const auto file = std::filesystem::u8path(path);
        if (!path.empty() && file.has_root_path() && !file.is_absolute())
            throw std::runtime_error("Project media path must be absolute or relative, not drive-relative.");
    }
    const auto duration = project.composition.durationSeconds;
    if (!std::isfinite(duration) || duration <= 0 || duration > 1200)
        throw std::runtime_error("Project duration must be positive and at most 20 minutes.");
    if (!std::isfinite(project.previewTimeSeconds) || project.previewTimeSeconds < 0 || project.previewTimeSeconds > duration)
        throw std::runtime_error("Project preview position is outside the timeline.");
}
void fields(const Json& json, std::initializer_list<const char*> names) {
    if (!json.is_object() || json.size() != names.size()) throw std::runtime_error("Project has missing or unknown fields.");
    for (auto name : names) if (!json.contains(name)) throw std::runtime_error(std::string("Missing project field: ") + name);
}
std::string relativeMedia(const std::string& path, const std::filesystem::path& base) {
    if (path.empty()) return {};
    const auto absolute = std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal();
    const auto relative = absolute.lexically_relative(base);
    return utf8(relative.empty() ? absolute : relative);
}
float framingNumber(const Json& object, const char* key, double minimum, double maximum) {
    if (!object.at(key).is_number()) throw std::runtime_error(std::string("Framing field must be numeric: ") + key);
    const double value = object.at(key).get<double>();
    if (!std::isfinite(value) || value < minimum || value > maximum)
        throw std::runtime_error(std::string("Framing field out of range: ") + key);
    return static_cast<float>(value);
}
std::string resolveMedia(const std::string& path, const std::filesystem::path& base) {
    if (path.empty()) return {};
    return utf8(std::filesystem::absolute(base / std::filesystem::u8path(path)).lexically_normal());
}
struct TemporaryFile {
    std::string path;
    bool committed = false;
    ~TemporaryFile() { if (!committed && !path.empty()) SDL_RemovePath(path.c_str()); }
};
}

bool isProjectPath(const std::string& path) {
    auto extension = utf8(std::filesystem::u8path(path).extension());
    for (auto& c : extension) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return extension == ".musdesk";
}

ProjectDocument parseProject(std::string_view source) {
    if (source.empty() || source.size() > maxBytes) throw std::runtime_error("Project file must contain 1-262144 bytes.");
    try {
        std::vector<std::unordered_set<std::string>> keys;
        const auto json = Json::parse(source.begin(), source.end(), [&](int depth, Json::parse_event_t event, Json& value) {
            if (depth > 24) throw std::runtime_error("Project JSON is nested too deeply.");
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
                throw std::runtime_error("Project JSON contains duplicate fields.");
            if (event == Json::parse_event_t::object_end) keys.pop_back();
            return true;
        });
        if (!json.at("version").is_number_integer() || json.at("version") < 1 || json.at("version") > 3)
            throw std::runtime_error("Unsupported project version; expected integer 1, 2 or 3.");
        const bool legacy = json.at("version") == 1;
        if (legacy) fields(json, {"version", "format", "title", "artist", "duration_seconds", "preview_time_seconds", "audio", "artwork", "preset"});
        else if (json.at("version") == 2) fields(json, {"version", "format", "short_edge", "title", "artist", "duration_seconds", "preview_time_seconds", "audio", "artwork", "preset"});
        else fields(json, {"version", "format", "short_edge", "title", "artist", "duration_seconds", "preview_time_seconds", "audio", "artwork", "preset", "framing"});
        if (legacy && json.at("format") != "9:16") throw std::runtime_error("Version 1 projects require 9:16 format.");
        if (!json.at("duration_seconds").is_number() || !json.at("preview_time_seconds").is_number())
            throw std::runtime_error("Project duration and preview position must be numbers.");
        ProjectDocument project;
        project.composition.format = parseCompositionFormat(json.at("format").get<std::string>());
        if (!legacy) {
            if (!json.at("short_edge").is_number_integer() || (json.at("short_edge") != 720 && json.at("short_edge") != 1080))
                throw std::runtime_error("Project short_edge must be integer 720 or 1080.");
            project.composition.shortEdge = json.at("short_edge").get<int>();
        }
        project.composition.title = json.at("title").get<std::string>();
        project.composition.artist = json.at("artist").get<std::string>();
        project.composition.durationSeconds = json.at("duration_seconds").get<double>();
        project.previewTimeSeconds = json.at("preview_time_seconds").get<double>();
        project.audioPath = json.at("audio").get<std::string>();
        project.artworkPath = json.at("artwork").get<std::string>();
        project.composition.preset = parsePreset(json.at("preset").dump());
        if (json.at("version") == 3) {
            const auto& framing = json.at("framing");
            fields(framing, {"background", "label", "softness"});
            const auto readFraming = [&](const Json& value) {
                fields(value, {"zoom", "focus_x", "focus_y"});
                ImageFraming result{framingNumber(value, "zoom", 1, 3),
                    framingNumber(value, "focus_x", 0, 1), framingNumber(value, "focus_y", 0, 1)};
                validateImageFraming(result);
                return result;
            };
            project.composition.backgroundFraming = readFraming(framing.at("background"));
            project.composition.labelFraming = readFraming(framing.at("label"));
            project.composition.backgroundSoftness = framingNumber(framing, "softness", 0, 1);
        }
        validate(project);
        return project;
    } catch (const Json::exception& error) { throw std::runtime_error(std::string("Invalid project JSON: ") + error.what()); }
}

std::string serializeProject(const ProjectDocument& project) {
    validate(project);
    try {
        const auto cropJson = [](const ImageFraming& crop) {
            return Json{{"zoom", crop.zoom}, {"focus_x", crop.focusX}, {"focus_y", crop.focusY}};
        };
        const Json json{{"version", 3}, {"format", formatName(project.composition.format)},
            {"short_edge", project.composition.shortEdge}, {"title", project.composition.title},
            {"artist", project.composition.artist}, {"duration_seconds", project.composition.durationSeconds},
            {"preview_time_seconds", project.previewTimeSeconds}, {"audio", project.audioPath},
            {"artwork", project.artworkPath}, {"preset", Json::parse(serializePreset(project.composition.preset))},
            {"framing", {{"background", cropJson(project.composition.backgroundFraming)},
                {"label", cropJson(project.composition.labelFraming)}, {"softness", project.composition.backgroundSoftness}}}};
        return json.dump(2) + '\n';
    } catch (const Json::exception& error) { throw std::runtime_error(std::string("Cannot serialize project: ") + error.what()); }
}

ProjectDocument loadProject(const std::string& path) {
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(SDL_IOFromFile(path.c_str(), "rb"), SDL_CloseIO);
    if (!stream) throw std::runtime_error(std::string("Cannot open project: ") + SDL_GetError());
    const auto size = SDL_GetIOSize(stream.get());
    if (size <= 0 || size > maxBytes) throw std::runtime_error("Project file must contain 1-262144 bytes.");
    std::string source(static_cast<std::size_t>(size), '\0');
    if (SDL_ReadIO(stream.get(), source.data(), source.size()) != source.size()) throw std::runtime_error("Cannot read complete project file.");
    auto project = parseProject(source);
    const auto base = std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().parent_path();
    project.audioPath = resolveMedia(project.audioPath, base);
    project.artworkPath = resolveMedia(project.artworkPath, base);
    return project;
}

void saveProject(const std::string& path, const ProjectDocument& project) {
    if (!isProjectPath(path)) throw std::runtime_error("Project files must use the .musdesk extension.");
    const auto destination = std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal();
    for (const auto& media : {project.audioPath, project.artworkPath}) {
        if (media.empty()) continue;
        const auto source = std::filesystem::absolute(std::filesystem::u8path(media)).lexically_normal();
        std::error_code error;
        if (destination == source || std::filesystem::equivalent(destination, source, error))
            throw std::runtime_error("A project cannot replace its source media.");
    }
    auto portable = project;
    portable.audioPath = relativeMedia(project.audioPath, destination.parent_path());
    portable.artworkPath = relativeMedia(project.artworkPath, destination.parent_path());
    const auto source = serializeProject(portable);
    static std::atomic<unsigned> counter{0};
    TemporaryFile temporary;
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> stream(nullptr, SDL_CloseIO);
    for (int attempt = 0; attempt < 8 && !stream; ++attempt) {
        temporary.path = path + ".tmp-" + std::to_string(SDL_GetTicksNS()) + "-" + std::to_string(counter++);
        stream.reset(SDL_IOFromFile(temporary.path.c_str(), "wbx"));
        if (!stream) temporary.path.clear();
    }
    if (!stream) throw std::runtime_error(std::string("Cannot create project file: ") + SDL_GetError());
    if (SDL_WriteIO(stream.get(), source.data(), source.size()) != source.size() || !SDL_FlushIO(stream.get()))
        throw std::runtime_error(std::string("Cannot write project: ") + SDL_GetError());
    if (!SDL_CloseIO(stream.release())) throw std::runtime_error(std::string("Cannot close project: ") + SDL_GetError());
    if (!SDL_RenamePath(temporary.path.c_str(), path.c_str())) throw std::runtime_error(std::string("Cannot replace project: ") + SDL_GetError());
    temporary.committed = true;
}
}
