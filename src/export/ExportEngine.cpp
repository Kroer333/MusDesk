#include "musdesk/export/ExportEngine.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace musdesk {
std::uint64_t exportFrameCount(std::uint64_t samples) {
    if (samples == 0 || samples > 48000ULL * 1200)
        throw std::runtime_error("Export requires audio between one sample and 20 minutes.");
    return (samples * 30 + 47999) / 48000;
}
double exportFrameTime(std::uint64_t index) { return static_cast<double>(index) / 30.0; }

namespace {
using Clock = std::chrono::steady_clock;
struct Process {
    SDL_Process* handle = nullptr;
    SDL_IOStream* input = nullptr;
    SDL_IOStream* output = nullptr;
    SDL_IOStream* errors = nullptr;
    std::string diagnostic;
    explicit Process(const std::vector<std::string>& args, bool writeInput) {
        std::vector<const char*> pointers;
        for (const auto& arg : args) pointers.push_back(arg.c_str());
        pointers.push_back(nullptr);
        const auto props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, pointers.data());
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, writeInput ? SDL_PROCESS_STDIO_APP : SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_APP);
        handle = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        if (!handle) throw std::runtime_error(std::string("Cannot start media tool: ") + SDL_GetError());
        const auto properties = SDL_GetProcessProperties(handle);
        input = writeInput ? SDL_GetProcessInput(handle) : nullptr;
        output = SDL_GetProcessOutput(handle);
        errors = static_cast<SDL_IOStream*>(SDL_GetPointerProperty(properties, SDL_PROP_PROCESS_STDERR_POINTER, nullptr));
    }
    ~Process() {
        if (handle) {
            if (!SDL_WaitProcess(handle, false, nullptr)) {
                SDL_KillProcess(handle, true);
                SDL_WaitProcess(handle, true, nullptr);
            }
            SDL_DestroyProcess(handle);
        }
    }
    void drain(SDL_IOStream* stream, std::string& text, std::size_t limit) {
        char buffer[4096];
        for (int i = 0; i < 16; ++i) {
            const auto count = SDL_ReadIO(stream, buffer, sizeof(buffer));
            if (count == 0) break;
            text.append(buffer, count);
            if (text.size() > limit) text.erase(0, text.size() - limit);
        }
    }
    void drainErrors() { drain(errors, diagnostic, 8192); }
    void closeInput() {
        if (input) { SDL_CloseIO(input); input = nullptr; }
    }
};
struct TemporaryFile {
    std::string path;
    bool committed = false;
    ~TemporaryFile() { if (!path.empty() && !committed) SDL_RemovePath(path.c_str()); }
    void reserve(const std::string& destination) {
        static std::atomic<unsigned> counter{0};
        for (int i = 0; i < 8; ++i) {
            const auto candidate = destination + ".tmp-" + std::to_string(SDL_GetTicksNS()) + "-" + std::to_string(counter++) + ".mp4";
            if (auto* stream = SDL_IOFromFile(candidate.c_str(), "wbx")) {
                path = candidate;
                if (!SDL_CloseIO(stream)) throw std::runtime_error("Cannot close temporary export file.");
                return;
            }
        }
        throw std::runtime_error(std::string("Cannot create export file: ") + SDL_GetError());
    }
};
void validateMovie(const std::string& text, std::uint64_t frames) {
    const auto json = nlohmann::json::parse(text);
    const auto& streams = json.at("streams");
    if (streams.size() != 2) throw std::runtime_error("MP4 must contain exactly one video and one audio stream.");
    const nlohmann::json* video = nullptr;
    const nlohmann::json* audio = nullptr;
    for (const auto& stream : streams) {
        if (stream.value("codec_type", "") == "video") video = &stream;
        if (stream.value("codec_type", "") == "audio") audio = &stream;
    }
    const double duration = exportFrameTime(frames);
    const auto nearDuration = [duration](const nlohmann::json& value) {
        const double seconds = std::stod(value.get<std::string>());
        return std::isfinite(seconds) && std::abs(seconds - duration) <= 0.06;
    };
    if (!video || !audio || video->value("codec_name", "") != "h264"
        || video->at("width") != 1080 || video->at("height") != 1920
        || video->value("pix_fmt", "") != "yuv420p" || video->value("avg_frame_rate", "") != "30/1"
        || std::stoull(video->at("nb_frames").get<std::string>()) != frames
        || audio->value("codec_name", "") != "aac" || audio->value("sample_rate", "") != "48000"
        || audio->at("channels") != 2 || !nearDuration(video->at("duration"))
        || !nearDuration(audio->at("duration")) || !nearDuration(json.at("format").at("duration")))
        throw std::runtime_error("Exported MP4 failed codec, dimensions, frame count or duration verification.");
    for (const auto* stream : {video, audio}) {
        const double start = std::stod(stream->at("start_time").get<std::string>());
        if (!std::isfinite(start) || std::abs(start) > 1.0 / 30)
            throw std::runtime_error("Exported streams do not start together at zero.");
    }
}
}

struct ExportEngine::Impl {
    VisualizerRenderer& renderer;
    CompositionSettings settings;
    std::shared_ptr<const DecodedAudio> audio;
    MediaTools tools;
    std::string destination;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::vector<std::uint8_t>> queue;
    ExportStatus state;
    std::uint64_t submitted = 0;
    std::atomic<bool> cancelled{false};
    std::jthread worker;

    Impl(VisualizerRenderer& target, CompositionSettings composition, std::shared_ptr<const DecodedAudio> clip,
        MediaTools mediaTools, std::string output)
        : renderer(target), settings(std::move(composition)), audio(std::move(clip)),
          tools(std::move(mediaTools)), destination(std::move(output)) {
        if (!audio || audio->samples.empty() || audio->samples.size() % DecodedAudio::channels)
            throw std::runtime_error("Import valid audio before exporting.");
        validatePreset(settings.preset);
        state.totalFrames = exportFrameCount(audio->samples.size() / DecodedAudio::channels);
        settings.durationSeconds = audio->durationSeconds();
        const auto outputFile = std::filesystem::absolute(std::filesystem::u8path(destination));
        auto extension = outputFile.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return SDL_tolower(c); });
        if (extension != ".mp4") throw std::runtime_error("Export destination must use the .mp4 extension.");
        if (!std::filesystem::is_regular_file(std::filesystem::u8path(audio->path)))
            throw std::runtime_error("The original audio file is no longer available.");
        if (std::filesystem::exists(outputFile) && std::filesystem::equivalent(outputFile, std::filesystem::u8path(audio->path)))
            throw std::runtime_error("Export cannot replace the source audio.");
        if (tools.ffmpegPath.empty() || tools.ffprobePath.empty()) throw std::runtime_error("FFmpeg and ffprobe are required for export.");
        worker = std::jthread([this] { run(); });
    }
    ~Impl() {
        cancelled = true;
        wake.notify_all();
        if (worker.joinable()) worker.join();
    }
    void checkCancel() {
        if (cancelled) throw std::runtime_error("Export cancelled.");
    }
    void run() {
        try {
            TemporaryFile temporary;
            temporary.reserve(destination);
            const auto count = state.totalFrames;
            Process encoder({tools.ffmpegPath, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", "1080x1920", "-framerate", "30", "-i", "pipe:0",
                "-protocol_whitelist", "file", "-i", audio->path, "-map", "0:v:0", "-map", "1:a:0",
                "-vf", "vflip", "-c:v", "libx264", "-preset", "fast", "-crf", "20", "-pix_fmt", "yuv420p",
                "-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2", "-af", "asetpts=PTS-STARTPTS,apad",
                "-t", std::to_string(exportFrameTime(count)), "-frames:v", std::to_string(count),
                "-movflags", "+faststart", temporary.path}, true);
            std::uint64_t written = 0;
            while (written < count) {
                checkCancel();
                std::vector<std::uint8_t> frame;
                {
                    std::unique_lock lock(mutex);
                    wake.wait_for(lock, std::chrono::milliseconds(5), [&] { return cancelled || !queue.empty(); });
                    if (!queue.empty()) { frame = std::move(queue.front()); queue.pop_front(); }
                }
                encoder.drainErrors();
                int exitCode;
                if (SDL_WaitProcess(encoder.handle, false, &exitCode))
                    throw std::runtime_error("FFmpeg exited before receiving all frames: " + encoder.diagnostic);
                if (frame.empty()) continue;
                std::size_t offset = 0;
                unsigned waitingWrites = 0;
                auto lastActivity = Clock::now();
                while (offset < frame.size()) {
                    checkCancel();
                    encoder.drainErrors();
                    const auto bytes = SDL_WriteIO(encoder.input, frame.data() + offset, std::min<std::size_t>(65536, frame.size() - offset));
                    offset += bytes;
                    if (bytes) {
                        lastActivity = Clock::now();
                        waitingWrites = 0;
                    }
                    else {
                        const auto status = SDL_GetIOStatus(encoder.input);
                        if (status != SDL_IO_STATUS_NOT_READY && status != SDL_IO_STATUS_READY)
                            throw std::runtime_error("FFmpeg input failed: " + encoder.diagnostic + " " + SDL_GetError());
                        if (SDL_WaitProcess(encoder.handle, false, &exitCode))
                            throw std::runtime_error("FFmpeg stopped encoding: " + encoder.diagnostic);
                        if (Clock::now() - lastActivity > std::chrono::seconds(120)) throw std::runtime_error("FFmpeg stopped accepting frames for 120 seconds.");
                        if (waitingWrites < 4) { ++waitingWrites; SDL_Delay(0); }
                        else SDL_DelayNS(100000);
                    }
                }
                ++written;
                std::lock_guard lock(mutex);
                state.framesWritten = written;
            }
            encoder.closeInput();
            const auto deadline = Clock::now() + std::chrono::seconds(120);
            int exitCode = -1;
            while (!SDL_WaitProcess(encoder.handle, false, &exitCode)) {
                checkCancel();
                encoder.drainErrors();
                if (Clock::now() > deadline) throw std::runtime_error("FFmpeg finalization timed out.");
                SDL_Delay(5);
            }
            encoder.drainErrors();
            if (exitCode != 0) throw std::runtime_error("FFmpeg encoding failed: " + encoder.diagnostic);
            checkCancel();
            {
                std::lock_guard lock(mutex);
                state.phase = ExportPhase::Verifying;
            }
            Process probe({tools.ffprobePath, "-v", "error", "-protocol_whitelist", "file", "-show_entries",
                "format=duration:stream=codec_type,codec_name,width,height,pix_fmt,avg_frame_rate,nb_frames,duration,start_time,sample_rate,channels",
                "-of", "json", "-i", temporary.path}, false);
            std::string metadata;
            const auto probeDeadline = Clock::now() + std::chrono::seconds(30);
            while (!SDL_WaitProcess(probe.handle, false, &exitCode)) {
                checkCancel();
                probe.drain(probe.output, metadata, 65536);
                probe.drainErrors();
                if (Clock::now() > probeDeadline) throw std::runtime_error("ffprobe export verification timed out.");
                SDL_Delay(5);
            }
            probe.drain(probe.output, metadata, 65536);
            probe.drainErrors();
            if (exitCode != 0) throw std::runtime_error("ffprobe failed: " + probe.diagnostic);
            validateMovie(metadata, count);
            // Serialize cancellation with the final replacement: a completed export wins afterwards.
            std::lock_guard lock(mutex);
            checkCancel();
            if (!SDL_RenamePath(temporary.path.c_str(), destination.c_str()))
                throw std::runtime_error(std::string("Cannot replace export destination: ") + SDL_GetError());
            temporary.committed = true;
            state.phase = ExportPhase::Completed;
            state.message = "MP4 verified and saved: " + destination;
        } catch (const std::exception& error) {
            std::lock_guard lock(mutex);
            if (state.phase != ExportPhase::Failed) {
                state.phase = cancelled ? ExportPhase::Cancelled : ExportPhase::Failed;
                state.message = error.what();
            }
            queue.clear();
        }
    }
};

ExportEngine::ExportEngine(VisualizerRenderer& renderer, CompositionSettings settings,
    std::shared_ptr<const DecodedAudio> audio, MediaTools tools, std::string outputPath)
    : impl_(std::make_unique<Impl>(renderer, std::move(settings), std::move(audio), std::move(tools), std::move(outputPath))) {}
ExportEngine::~ExportEngine() = default;
void ExportEngine::cancel() {
    std::lock_guard lock(impl_->mutex);
    if (impl_->state.active()) impl_->cancelled = true;
    impl_->wake.notify_all();
}
ExportStatus ExportEngine::status() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->state;
}
void ExportEngine::update() {
    auto& task = *impl_;
    {
        std::lock_guard lock(task.mutex);
        if (!task.state.active() || task.cancelled || task.submitted >= task.state.totalFrames || task.queue.size() >= 2) return;
    }
    try {
        const auto time = exportFrameTime(task.submitted);
        task.renderer.renderFrame(time, task.settings, task.audio->envelope.at(time));
        auto pixels = task.renderer.readPixels();
        std::lock_guard lock(task.mutex);
        if (task.state.active() && !task.cancelled) {
            task.queue.push_back(std::move(pixels));
            ++task.submitted;
            task.wake.notify_one();
        }
    } catch (const std::exception& error) {
        std::lock_guard lock(task.mutex);
        task.state.phase = ExportPhase::Failed;
        task.state.message = std::string("Export rendering failed: ") + error.what();
        task.cancelled = true;
        task.wake.notify_all();
    }
}
}
