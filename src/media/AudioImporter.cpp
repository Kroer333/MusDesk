#include "musdesk/media/AudioImporter.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace musdesk {
namespace {
std::string utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}

std::string run(const std::vector<std::string>& arguments) {
    std::vector<const char*> argv;
    for (const auto& argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    // Separate arguments let SDL perform platform quoting; no shell is involved.
    const auto properties = SDL_CreateProperties();
    if (!properties) throw std::runtime_error(SDL_GetError());
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
        SDL_CreateProcessWithProperties(properties), SDL_DestroyProcess);
    SDL_DestroyProperties(properties);
    if (!process) throw std::runtime_error(std::string("Cannot start media tool: ") + SDL_GetError());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    std::string output;
    auto* stream = SDL_GetProcessOutput(process.get());
    int exitCode = -1;
    bool exited = false;
    try {
        if (!stream) throw std::runtime_error("Cannot open media tool output stream.");
        for (;;) {
            std::array<char, 4096> buffer{};
            const auto bytes = SDL_ReadIO(stream, buffer.data(), buffer.size());
            output.append(buffer.data(), bytes);
            if (output.size() > 64 * 1024) throw std::runtime_error("Media tool output exceeds 64 KiB.");
            const auto status = SDL_GetIOStatus(stream);
            if (status == SDL_IO_STATUS_ERROR) throw std::runtime_error("Cannot read media tool output.");
            if (!exited) exited = SDL_WaitProcess(process.get(), false, &exitCode);
            if (exited && status == SDL_IO_STATUS_EOF) break;
            if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Media tool timed out after 15 seconds.");
            if (!bytes) SDL_Delay(5);
        }
    } catch (...) {
        if (!exited) {
            SDL_KillProcess(process.get(), true);
            SDL_WaitProcess(process.get(), true, nullptr);
        }
        throw;
    }
    if (exitCode != 0) throw std::runtime_error("Media tool failed; the file may be damaged or unsupported.");
    return output;
}

std::string findTool(const std::string& name, const std::vector<std::filesystem::path>& directories) {
    for (const auto& directory : directories) {
        auto candidate = directory / name;
        std::error_code error;
        if (!std::filesystem::is_regular_file(candidate, error)) continue;
        try {
            candidate = std::filesystem::absolute(candidate);
            const auto path = utf8(candidate);
            if (run({path, "-version"}).starts_with(name.substr(0, name.find('.')) + " version ")) return path;
        } catch (const std::exception&) {
            // An unusable candidate must not hide another valid installation.
        }
    }
    return {};
}
}

MediaTools detectMediaTools() {
    std::vector<std::filesystem::path> directories;
    if (const auto* configured = SDL_getenv("MUSDESK_FFMPEG_DIR"); configured && *configured)
        directories.push_back(std::filesystem::u8path(configured));
    if (const auto* base = SDL_GetBasePath()) directories.push_back(std::filesystem::u8path(base));
    if (const auto* path = SDL_getenv("PATH")) {
        std::string_view remaining(path);
#ifdef _WIN32
        constexpr char separator = ';';
#else
        constexpr char separator = ':';
#endif
        while (!remaining.empty()) {
            const auto end = remaining.find(separator);
            auto entry = remaining.substr(0, end);
            if (entry.size() >= 2 && entry.front() == '"' && entry.back() == '"') entry = entry.substr(1, entry.size()-2);
            if (!entry.empty()) directories.push_back(std::filesystem::u8path(entry));
            if (end == std::string_view::npos) break;
            remaining.remove_prefix(end + 1);
        }
    }
#ifdef _WIN32
    const std::string suffix = ".exe";
#else
    const std::string suffix;
#endif
    MediaTools result;
    result.ffmpegPath = findTool("ffmpeg" + suffix, directories);
    result.ffprobePath = findTool("ffprobe" + suffix, directories);
    result.status = "FFmpeg: " + std::string(result.ffmpegPath.empty() ? "missing or unusable" : "ready")
        + " | ffprobe: " + (result.ffprobePath.empty() ? "missing or unusable" : "ready");
    return result;
}

bool isAudioPath(const std::string& path) {
    auto extension = utf8(std::filesystem::u8path(path).extension());
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    return extension == ".mp3" || extension == ".wav" || extension == ".ogg";
}

AudioMetadata importAudio(const std::string& path, const MediaTools& tools) {
    if (tools.ffprobePath.empty()) throw std::runtime_error("ffprobe is unavailable. Set MUSDESK_FFMPEG_DIR or add its directory to PATH, then restart MusDesk.");
    if (!isAudioPath(path)) throw std::runtime_error("Choose an MP3, WAV or OGG audio file.");
    const auto file = std::filesystem::absolute(std::filesystem::u8path(path));
    if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Audio file does not exist or is not a regular file.");
    const auto absolutePath = utf8(file);
    const auto output = run({tools.ffprobePath, "-v", "error", "-protocol_whitelist", "file", "-select_streams", "a:0",
        "-show_entries", "stream=codec_type:format=duration", "-of", "default=noprint_wrappers=1", "-i", absolutePath});
    bool hasAudio = false;
    double duration = 0;
    std::string_view lines(output);
    while (!lines.empty()) {
        const auto end = lines.find('\n');
        auto line = lines.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line == "codec_type=audio") hasAudio = true;
        if (line.starts_with("duration=")) {
            line.remove_prefix(9);
            const auto parsed = std::from_chars(line.data(), line.data() + line.size(), duration);
            if (parsed.ec != std::errc{} || parsed.ptr != line.data() + line.size()) duration = 0;
        }
        if (end == std::string_view::npos) break;
        lines.remove_prefix(end + 1);
    }
    if (!hasAudio || !std::isfinite(duration) || duration <= 0)
        throw std::runtime_error("The file has no audio stream with a known positive duration.");
    return {absolutePath, duration};
}
}
