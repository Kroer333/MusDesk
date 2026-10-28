#include "musdesk/export/ExportEngine.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Context {
    SDL_Window* window = nullptr;
    SDL_GLContext gl = nullptr;
    ~Context() {
        if (gl) SDL_GL_DestroyContext(gl);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};
std::string utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
void wav(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    const auto integer = [&](std::uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) file.put(static_cast<char>(value >> (i * 8)));
    };
    // Non-integral frame duration exercises rounding and the final padded frame.
    constexpr unsigned samples = 52801;
    file.write("RIFF", 4); integer(36 + samples * 4, 4); file.write("WAVEfmt ", 8);
    integer(16, 4); integer(1, 2); integer(2, 2); integer(48000, 4); integer(192000, 4);
    integer(4, 2); integer(16, 2); file.write("data", 4); integer(samples * 4, 4);
    for (unsigned i = 0; i < samples; ++i) {
        const double amplitude = (i < 4800 || i >= 24000 && i < 28800) ? 0.6 : 0;
        const auto sample = static_cast<std::int16_t>(amplitude * 32767 * std::sin(i * 70.0 * 6.283185307179586 / 48000));
        integer(static_cast<std::uint16_t>(sample), 2); integer(static_cast<std::uint16_t>(sample), 2);
    }
    require(static_cast<bool>(file), "Cannot write export audio fixture");
}
musdesk::ExportStatus finish(musdesk::ExportEngine& job) {
    const auto start = SDL_GetTicks();
    auto previous = job.status();
    while (previous.active()) {
        job.update();
        const auto next = job.status();
        require(next.framesWritten >= previous.framesWritten && next.framesWritten <= next.totalFrames, "Export progress must be monotonic");
        previous = next;
        if (SDL_GetTicks() - start > 120000) throw std::runtime_error("Export test timed out");
        SDL_Delay(1);
    }
    return previous;
}
void preserved(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), {});
    require(text == "existing destination", "A failed/cancelled export replaced the destination");
}
void runTool(const std::vector<std::string>& args) {
    std::vector<const char*> pointers;
    for (const auto& arg : args) pointers.push_back(arg.c_str());
    pointers.push_back(nullptr);
    auto* process = SDL_CreateProcess(pointers.data(), false);
    require(process != nullptr, "Cannot start test decoding process");
    const auto deadline = SDL_GetTicks() + 30000;
    int code = -1;
    while (!SDL_WaitProcess(process, false, &code)) {
        if (SDL_GetTicks() > deadline) {
            SDL_KillProcess(process, true);
            SDL_WaitProcess(process, true, nullptr);
            SDL_DestroyProcess(process);
            throw std::runtime_error("Test decoding timed out");
        }
        SDL_Delay(5);
    }
    SDL_DestroyProcess(process);
    require(code == 0, "Exported movie could not be decoded");
}
double rmsAt(const musdesk::DecodedAudio& audio, double time) {
    const auto start = static_cast<std::size_t>(time * 48000) * 2;
    double energy = 0;
    for (std::size_t i = start; i < start + 960; ++i) {
        require(i < audio.samples.size(), "Decoded output audio is too short");
        const double value = audio.samples[i] / 32768.0;
        energy += value * value;
    }
    return std::sqrt(energy / 960);
}
}
int main(int argc, char* argv[]) {
    Context context;
    try {
        require(musdesk::exportFrameCount(1) == 1, "A sample needs one frame");
        require(musdesk::exportFrameCount(48000) == 30, "One second needs 30 frames");
        require(musdesk::exportFrameCount(48001) == 31, "A partial frame must round up");
        require(musdesk::exportFrameCount(48000ULL * 1200) == 36000, "Maximum duration frame count");
        require(musdesk::exportFrameTime(31) == 31 / 30.0, "Timestamp must be i / 30.0");
        for (const auto invalid : {0ULL, 48000ULL * 1200 + 1, ~0ULL}) {
            bool rejected = false;
            try { musdesk::exportFrameCount(invalid); } catch (const std::exception&) { rejected = true; }
            require(rejected, "Invalid/overflowing sample count accepted");
        }
        if (argc > 1 && std::string(argv[1]) == "--timeline") { std::cout << "Export timeline checks passed.\n"; return 0; }
        require(SDL_Init(SDL_INIT_VIDEO), "SDL initialization failed");
        const auto tools = musdesk::detectMediaTools();
        if (tools.ffmpegPath.empty() || tools.ffprobePath.empty()) { std::cout << "Export integration requires FFmpeg and ffprobe.\n"; return 77; }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        context.window = SDL_CreateWindow("MusDesk export checks", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        require(context.window != nullptr, "Cannot create hidden export window");
        context.gl = SDL_GL_CreateContext(context.window);
        require(context.gl && SDL_GL_MakeCurrent(context.window, context.gl), "Cannot create current GL context");
        musdesk::VisualizerRenderer renderer(std::string(SDL_GetBasePath()) + "assets/Roboto-Medium.ttf");
        renderer.setArtwork({2, 2, {255,40,10,255, 255,40,10,255, 10,40,255,255, 10,40,255,255}});
        const auto folder = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures" / "export";
        std::filesystem::create_directories(folder);
        if ((argc == 5 || argc == 6) && std::string(argv[1]) == "--benchmark") {
            auto audio = std::make_shared<musdesk::DecodedAudio>(musdesk::prepareAudio(argv[2], tools));
            const auto seconds = std::stoul(argv[4]);
            require(seconds >= 1 && seconds <= 60, "Benchmark duration must be 1-60 seconds");
            audio->samples.resize(std::min(audio->samples.size(), seconds * 48000ULL * 2));
            audio->envelope = musdesk::analyzeRms(audio->samples, 48000, 2);
            audio->rhythm = musdesk::analyzeRhythm(audio->samples, 48000, 2);
            renderer.setArtwork(musdesk::loadArtwork(argv[3]));
            musdesk::CompositionSettings composition;
            composition.title = "Export benchmark";
            composition.durationSeconds = audio->durationSeconds();
            const auto started = SDL_GetTicksNS();
            musdesk::ExportEngine job(renderer, composition, audio, tools, utf8(folder / "benchmark.mp4"),
                argc == 6 && std::string(argv[5]) == "cpu" ? musdesk::ExportEncoder::Cpu : musdesk::ExportEncoder::Automatic);
            const auto result = finish(job);
            require(result.phase == musdesk::ExportPhase::Completed, result.message.c_str());
            const double elapsed = (SDL_GetTicksNS() - started) / 1e9;
            std::cout << result.totalFrames << " frames in " << elapsed << " s; "
                << result.totalFrames / elapsed << " FPS; " << result.encoder << '\n';
            return 0;
        }
        const auto input = folder / std::filesystem::path(L"d\u017Awi\u0119k testowy.wav");
        wav(input);
        auto clip = std::make_shared<musdesk::DecodedAudio>(musdesk::prepareAudio(utf8(input), tools));
        musdesk::validateAudioSource(*clip);
        const auto rejects = [](auto action) {
            try { action(); } catch (const std::exception&) { return true; }
            return false;
        };
        require(rejects([&] { musdesk::validateExportDestination(utf8(folder / "missing-folder" / "out.mp4"), clip->path); }), "Missing export directory accepted");
        require(rejects([&] { musdesk::validateExportDestination(utf8(folder / "out.avi"), clip->path); }), "Unsupported output extension accepted");
        const auto protectedImage = folder / "source-image.mp4";
        { std::ofstream file(protectedImage); file << "source artwork"; }
        require(rejects([&] { musdesk::validateExportDestination(utf8(protectedImage), clip->path, utf8(protectedImage)); }), "Source artwork may be overwritten");
        const auto directoryTarget = folder / "directory.mp4";
        std::filesystem::create_directories(directoryTarget);
        require(rejects([&] { musdesk::validateExportDestination(utf8(directoryTarget), clip->path); }), "Directory accepted as export file");
        const auto savedTime = std::filesystem::last_write_time(input);
        std::filesystem::last_write_time(input, savedTime + std::chrono::seconds(2));
        require(rejects([&] { musdesk::validateAudioSource(*clip); }), "Changed audio timestamp was accepted");
        std::filesystem::last_write_time(input, savedTime);
        { std::ofstream file(input, std::ios::binary | std::ios::app); file.put('\0'); }
        require(rejects([&] { musdesk::validateAudioSource(*clip); }), "Changed audio size was accepted");
        wav(input);
        clip = std::make_shared<musdesk::DecodedAudio>(musdesk::prepareAudio(utf8(input), tools));
        auto missingClip = *clip;
        missingClip.path = utf8(folder / "missing.wav");
        require(rejects([&] { musdesk::validateAudioSource(missingClip); }), "Missing source audio was accepted");
        musdesk::CompositionSettings settings;
        settings.preset = musdesk::beatPulsePreset();
        settings.durationSeconds = clip->durationSeconds();
        const auto output = folder / std::filesystem::path(L"gotowy film.mp4");
        { std::ofstream file(output); file << "previous export"; }
        {
            musdesk::ExportEngine job(renderer, settings, clip, tools, utf8(output));
            const auto status = finish(job);
            require(status.phase == musdesk::ExportPhase::Completed, status.message.c_str());
            require(status.framesWritten == 34, "Incorrect final frame count");
            std::cout << status.message << '\n';
        }
        const auto decodedPath = folder / "decoded.wav";
        const auto pixelsPath = folder / "first-frame.rgba";
        runTool({tools.ffmpegPath, "-v", "error", "-xerror", "-nostdin", "-y", "-i", utf8(output),
            "-map", "0:v:0", "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgba", utf8(pixelsPath),
            "-map", "0:a:0", "-c:a", "pcm_s16le", utf8(decodedPath)});
        const auto decoded = musdesk::prepareAudio(utf8(decodedPath), tools);
        require(rmsAt(decoded, 0.03) > 0.3 && rmsAt(decoded, 0.53) > 0.3,
            "Audio pulses shifted from their source timestamps");
        require(rmsAt(decoded, 0.3) < 0.005 && rmsAt(decoded, 0.9) < 0.005, "Audio silence intervals shifted");
        renderer.renderFrame(0, settings, clip->reactionAt(0, settings.preset.beatThreshold));
        const auto reference = renderer.readPixels();
        std::ifstream pixelFile(pixelsPath, std::ios::binary);
        const std::vector<unsigned char> pixels((std::istreambuf_iterator<char>(pixelFile)), {});
        require(pixels.size() == reference.size(), "Decoded video frame has wrong dimensions");
        for (const int y : {100, 1800}) {
            const auto encodedIndex = (y * 1080 + 540) * 4;
            const auto referenceIndex = ((1919 - y) * 1080 + 540) * 4;
            for (int c = 0; c < 3; ++c)
                require(std::abs(int(pixels[encodedIndex + c]) - int(reference[referenceIndex + c])) < 18,
                    "Exported video is vertically inverted or differs from the renderer");
        }
        const auto reactivePath = folder / "reactive-frame.rgba";
        runTool({tools.ffmpegPath, "-v", "error", "-xerror", "-nostdin", "-y", "-i", utf8(output),
            "-vf", "select=eq(n\\,3)", "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgba", utf8(reactivePath)});
        const auto reaction = clip->reactionAt(musdesk::exportFrameTime(3), settings.preset.beatThreshold);
        require(reaction.beat > 0 && reaction.bass > 0, "Export fixture must exercise bass and beat effects");
        renderer.renderFrame(musdesk::exportFrameTime(3), settings, reaction);
        const auto reactiveReference = renderer.readPixels();
        renderer.renderFrame(musdesk::exportFrameTime(3), settings, reaction.energy);
        const auto rmsOnlyReference = renderer.readPixels();
        std::ifstream reactiveFile(reactivePath, std::ios::binary);
        const std::vector<unsigned char> reactivePixels((std::istreambuf_iterator<char>(reactiveFile)), {});
        require(reactivePixels.size() == reactiveReference.size(), "Reactive export frame has wrong dimensions");
        double reactiveError = 0, rmsOnlyError = 0;
        std::size_t comparisons = 0;
        for (int y = 0; y < 1920; y += 8) for (int x = 0; x < 1080; x += 8) {
            const auto encoded = (y * 1080 + x) * 4;
            const auto expected = ((1919 - y) * 1080 + x) * 4;
            for (int c = 0; c < 3; ++c) {
                reactiveError += std::abs(int(reactivePixels[encoded + c]) - int(reactiveReference[expected + c]));
                rmsOnlyError += std::abs(int(reactivePixels[encoded + c]) - int(rmsOnlyReference[expected + c]));
                ++comparisons;
            }
        }
        require(reactiveError / comparisons < 12 && reactiveError < rmsOnlyError,
            "Exported beat-reactive frame must match preview more closely than the RMS-only frame");
        std::cout << "Reactive export frame mean error: " << reactiveError / comparisons
            << "; RMS-only mismatch: " << rmsOnlyError / comparisons << '\n';
        runTool({tools.ffmpegPath, "-v", "error", "-xerror", "-nostdin", "-i", utf8(output), "-f", "null", "-"});
        {
            musdesk::ExportEngine job(renderer, settings, clip, tools, utf8(folder / "cpu.mp4"), musdesk::ExportEncoder::Cpu);
            const auto status = finish(job);
            require(status.phase == musdesk::ExportPhase::Completed && status.encoder == "CPU (x264 veryfast)", "Explicit CPU export failed");
            require(status.elapsedSeconds > 0, "Export duration was not reported");
        }
        const auto guarded = folder / "preserved.mp4";
        { std::ofstream file(guarded); file << "existing destination"; }
        {
            musdesk::ExportEngine job(renderer, settings, clip, tools, utf8(guarded), musdesk::ExportEncoder::Cpu);
            const auto deadline = SDL_GetTicks() + 15000;
            while (job.status().active() && job.status().framesWritten == 0) {
                job.update();
                require(SDL_GetTicks() < deadline, "Encoder did not begin accepting frames");
                SDL_Delay(1);
            }
            job.cancel();
            require(finish(job).phase == musdesk::ExportPhase::Cancelled, "Cancellation was not reported");
        }
        preserved(guarded);
        {
            musdesk::ExportEngine job(renderer, settings, clip, tools, utf8(guarded));
            job.update();
        }
        preserved(guarded);
        {
            auto broken = tools;
            broken.ffmpegPath = tools.ffprobePath; // A real process exits with unsupported encoding arguments.
            musdesk::ExportEngine job(renderer, settings, clip, broken, utf8(guarded));
            const auto status = finish(job);
            require(status.phase == musdesk::ExportPhase::Failed && !status.message.empty(), "Encoder failure was not reported");
        }
        preserved(guarded);
        {
            auto broken = tools;
            broken.ffprobePath = "missing-musdesk-ffprobe";
            musdesk::ExportEngine job(renderer, settings, clip, broken, utf8(guarded));
            require(finish(job).phase == musdesk::ExportPhase::Failed, "Verification failure was not reported");
        }
        preserved(guarded);
        for (const auto& file : std::filesystem::directory_iterator(folder))
            require(file.path().filename().string().find(".tmp-") == std::string::npos, "Export left a partial temporary file");
        require(glGetError() == GL_NO_ERROR, "OpenGL error during export");
        std::cout << "Export, orientation, audio pulse timing, full decoding, verification, cancellation, shutdown, process errors and destination preservation passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << " " << SDL_GetError() << '\n';
        return 1;
    }
}
