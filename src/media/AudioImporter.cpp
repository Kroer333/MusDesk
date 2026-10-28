#include "musdesk/media/AudioImporter.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <bit>
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
DecodedAudio::SourceStamp audioSourceStamp(const std::string& path) {
    const auto file = std::filesystem::u8path(path);
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error))
        throw std::runtime_error("Original audio is missing or inaccessible. Import it again before exporting.");
    const auto size = std::filesystem::file_size(file, error);
    if (error) throw std::runtime_error("Cannot inspect original audio. Check file access and import it again.");
    const auto modified = std::filesystem::last_write_time(file, error);
    if (error) throw std::runtime_error("Cannot inspect original audio modification time.");
    return {size, modified};
}

std::string run(const std::vector<std::string>& arguments, std::stop_token cancellation = {},
    std::size_t maxBytes = 64 * 1024, int timeoutSeconds = 15) {
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
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    std::string output;
    auto* stream = SDL_GetProcessOutput(process.get());
    int exitCode = -1;
    bool exited = false;
    try {
        if (!stream) throw std::runtime_error("Cannot open media tool output stream.");
        for (;;) {
            if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
            std::array<char, 32768> buffer{};
            const auto bytes = SDL_ReadIO(stream, buffer.data(), buffer.size());
            output.append(buffer.data(), bytes);
            if (output.size() > maxBytes) throw std::runtime_error("Media tool output exceeds the memory limit.");
            const auto status = SDL_GetIOStatus(stream);
            if (status == SDL_IO_STATUS_ERROR) throw std::runtime_error("Cannot read media tool output.");
            if (!exited) exited = SDL_WaitProcess(process.get(), false, &exitCode);
            if (exited && status == SDL_IO_STATUS_EOF) break;
            if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Media tool timed out.");
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

std::string findTool(const std::string& name, const std::vector<std::filesystem::path>& directories, std::stop_token cancellation) {
    for (const auto& directory : directories) {
        if (cancellation.stop_requested()) throw std::runtime_error("Media tool detection cancelled.");
        auto candidate = directory / name;
        std::error_code error;
        if (!std::filesystem::is_regular_file(candidate, error)) continue;
        try {
            candidate = std::filesystem::absolute(candidate);
            const auto path = utf8(candidate);
            if (run({path, "-version"}, cancellation).starts_with(name.substr(0, name.find('.')) + " version ")) return path;
        } catch (const std::exception&) {
            if (cancellation.stop_requested()) throw;
            // An unusable candidate must not hide another valid installation.
        }
    }
    return {};
}
}

MediaTools detectMediaTools(std::stop_token cancellation) {
    if (cancellation.stop_requested()) throw std::runtime_error("Media tool detection cancelled.");
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
    result.ffmpegPath = findTool("ffmpeg" + suffix, directories, cancellation);
    result.ffprobePath = findTool("ffprobe" + suffix, directories, cancellation);
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

AudioMetadata importAudio(const std::string& path, const MediaTools& tools, std::stop_token cancellation) {
    if (tools.ffprobePath.empty()) throw std::runtime_error("ffprobe is unavailable. Set MUSDESK_FFMPEG_DIR or add its directory to PATH, then restart MusDesk.");
    if (!isAudioPath(path)) throw std::runtime_error("Choose an MP3, WAV or OGG audio file.");
    const auto file = std::filesystem::absolute(std::filesystem::u8path(path));
    if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Audio file does not exist or is not a regular file.");
    const auto absolutePath = utf8(file);
    const auto output = run({tools.ffprobePath, "-v", "error", "-protocol_whitelist", "file", "-select_streams", "a:0",
        "-show_entries", "stream=codec_type:format=duration", "-of", "default=noprint_wrappers=1", "-i", absolutePath}, cancellation);
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

DecodedAudio prepareAudio(const std::string& path, const MediaTools& tools, std::stop_token cancellation) {
    if (tools.ffmpegPath.empty()) throw std::runtime_error("FFmpeg is unavailable. Install FFmpeg and restart MusDesk.");
    if (cancellation.stop_requested()) throw std::runtime_error("Audio import cancelled.");
    const auto metadata = importAudio(path, tools, cancellation);
    constexpr double maxDuration = 20 * 60;
    if (metadata.durationSeconds > maxDuration) throw std::runtime_error("Audio preview currently supports tracks up to 20 minutes.");
    DecodedAudio audio;
    audio.path = metadata.path;
    audio.sourceStamp = audioSourceStamp(audio.path);
    {
        constexpr std::size_t maxBytes = 48000ULL * 2 * sizeof(std::int16_t) * 1200;
        const auto pcm = run({tools.ffmpegPath, "-v", "error", "-xerror", "-nostdin", "-protocol_whitelist", "file",
            "-i", metadata.path, "-map", "0:a:0", "-vn", "-sn", "-dn", "-ac", "2", "-ar", "48000",
            "-c:a", "pcm_s16le", "-f", "s16le", "pipe:1"}, cancellation, maxBytes, 120);
        if (pcm.empty() || pcm.size() % (2 * sizeof(std::int16_t)) != 0)
            throw std::runtime_error("FFmpeg returned incomplete PCM audio.");
        audio.samples.resize(pcm.size() / sizeof(std::int16_t));
        std::memcpy(audio.samples.data(), pcm.data(), pcm.size());
        if constexpr (std::endian::native == std::endian::big) {
            for (auto& sample : audio.samples) {
                const auto value = static_cast<std::uint16_t>(sample);
                sample = static_cast<std::int16_t>((value >> 8) | (value << 8));
            }
        }
    }
    audio.envelope = analyzeRms(audio.samples, DecodedAudio::sampleRate, DecodedAudio::channels, cancellation);
    audio.waveform = analyzeWaveform(audio.samples, DecodedAudio::channels, cancellation);
    audio.rhythm = analyzeRhythm(audio.samples, DecodedAudio::sampleRate, DecodedAudio::channels, cancellation);
    audio.spectrum = analyzeSpectrum(audio.samples, DecodedAudio::sampleRate, DecodedAudio::channels, cancellation);
    validateAudioSource(audio);
    return audio;
}
void validateAudioSource(const DecodedAudio& audio) {
    const auto current = audioSourceStamp(audio.path);
    if (!audio.sourceStamp || current != *audio.sourceStamp)
        throw std::runtime_error("Original audio changed after import. Import it again to keep video and audio synchronized.");
}
}
