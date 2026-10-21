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
        const auto sample = static_cast<std::int16_t>(amplitude * 32767 * std::sin(i * 440.0 * 6.283185307179586 / 48000));
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
        const auto input = folder / std::filesystem::path(L"d\u017Awi\u0119k testowy.wav");
        wav(input);
        auto clip = std::make_shared<musdesk::DecodedAudio>(musdesk::prepareAudio(utf8(input), tools));
        musdesk::CompositionSettings settings;
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
        renderer.renderFrame(0, settings, clip->envelope.at(0));
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
        runTool({tools.ffmpegPath, "-v", "error", "-xerror", "-nostdin", "-i", utf8(output), "-f", "null", "-"});
        const auto guarded = folder / "preserved.mp4";
        { std::ofstream file(guarded); file << "existing destination"; }
        {
            musdesk::ExportEngine job(renderer, settings, clip, tools, utf8(guarded));
            job.update();
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
