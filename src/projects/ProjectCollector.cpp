#include "musdesk/projects/ProjectCollector.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace musdesk {
namespace {
namespace fs = std::filesystem;
std::string utf8(const fs::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
void checkCancellation(std::stop_token token) {
    if (token.stop_requested()) throw std::runtime_error("Project collection cancelled.");
}
struct Collection {
    fs::path folder;
    std::vector<fs::path> files;
    bool completed = false;
    ~Collection() {
        if (completed || folder.empty()) return;
        std::error_code error;
        for (const auto& file : files) fs::remove(file, error);
        fs::remove(folder / "media", error);
        fs::remove(folder, error);
    }
};
void copyMedia(const fs::path& source, const fs::path& destination, Collection& collection,
    std::stop_token cancellation) {
    const auto size = fs::file_size(source);
    const auto modified = fs::last_write_time(source);
    using Stream = std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)>;
    Stream input(SDL_IOFromFile(utf8(source).c_str(), "rb"), SDL_CloseIO);
    if (!input) throw std::runtime_error("Cannot read source media: " + utf8(source));
    Stream output(SDL_IOFromFile(utf8(destination).c_str(), "wbx"), SDL_CloseIO);
    if (!output) throw std::runtime_error("Cannot create collected media: " + utf8(destination));
    collection.files.push_back(destination);
    std::array<char, 256 * 1024> buffer;
    std::uintmax_t copied = 0;
    while (copied < size) {
        checkCancellation(cancellation);
        const auto count = static_cast<std::size_t>(std::min<std::uintmax_t>(buffer.size(), size - copied));
        if (SDL_ReadIO(input.get(), buffer.data(), count) != count
            || SDL_WriteIO(output.get(), buffer.data(), count) != count)
            throw std::runtime_error("Cannot copy complete media file: " + utf8(source));
        copied += count;
    }
    if (!SDL_FlushIO(output.get()) || !SDL_CloseIO(output.release()))
        throw std::runtime_error("Cannot finish collected media: " + utf8(destination));
    if (fs::file_size(source) != size || fs::last_write_time(source) != modified)
        throw std::runtime_error("Source media changed during collection: " + utf8(source));
}
}

std::string collectProject(const std::string& parentDirectory, const ProjectDocument& project,
    std::stop_token cancellation) {
    serializeProject(project);
    checkCancellation(cancellation);
    if (parentDirectory.empty()) throw std::runtime_error("Choose a destination folder for the collected project.");
    const auto parent = fs::canonical(fs::u8path(parentDirectory));
    if (!fs::is_directory(parent)) throw std::runtime_error("Collection destination must be an existing folder.");
    struct Media { fs::path source; std::string* reference; const char* name; };
    auto portable = project;
    Media media[] = {{fs::u8path(project.audioPath), &portable.audioPath, "audio"},
        {fs::u8path(project.artworkPath), &portable.artworkPath, "artwork"}};
    for (auto& item : media) {
        if (item.source.empty()) continue;
        item.source = fs::absolute(item.source).lexically_normal();
        if (!fs::is_regular_file(item.source))
            throw std::runtime_error("Locate missing media before collecting: " + utf8(item.source));
    }
    Collection collection;
    for (int suffix = 1; suffix <= 10000; ++suffix) {
        const auto name = "MusDesk-project" + (suffix == 1 ? std::string{} : "-" + std::to_string(suffix));
        const auto candidate = parent / name;
        if (fs::exists(candidate)) continue;
        if (fs::create_directory(candidate)) { collection.folder = candidate; break; }
    }
    if (collection.folder.empty()) throw std::runtime_error("Cannot reserve a new project folder.");
    fs::create_directory(collection.folder / "media");
    for (auto& item : media) {
        if (item.source.empty()) continue;
        const auto destination = collection.folder / "media" / (item.name + utf8(item.source.extension()));
        copyMedia(item.source, destination, collection, cancellation);
        *item.reference = utf8(destination);
    }
    checkCancellation(cancellation);
    const auto document = collection.folder / "project.musdesk";
    saveProject(utf8(document), portable);
    collection.completed = true;
    return utf8(document);
}
}
