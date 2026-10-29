#include "musdesk/media/AudioImporter.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <chrono>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "-version") {
        std::ofstream(std::filesystem::u8path(SDL_GetBasePath()) / "started") << "ready";
        SDL_Delay(30000);
        return 0;
    }
    try {
        std::stop_source cancelled;
        cancelled.request_stop();
        bool detectionCancelled = false;
        try { (void)musdesk::detectMediaTools(cancelled.get_token()); }
        catch (const std::exception&) { detectionCancelled = true; }
        require(detectionCancelled, "Pre-cancelled tool detection must stop");
#ifdef _WIN32
        const auto base = std::filesystem::u8path(SDL_GetBasePath());
        const auto folder = base / "test-fixtures" / ("detection-"
            + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder);
        std::filesystem::copy_file(base / "MusDeskAudioTests.exe", folder / "ffmpeg.exe");
        std::filesystem::copy_file(base / "SDL3.dll", folder / "SDL3.dll");
        const auto directoryBytes = folder.u8string();
        const std::string directory(directoryBytes.begin(), directoryBytes.end());
        const char* oldDirectory = SDL_getenv("MUSDESK_FFMPEG_DIR");
        const std::optional<std::string> previous = oldDirectory ? std::optional<std::string>(oldDirectory) : std::nullopt;
        require(SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "MUSDESK_FFMPEG_DIR", directory.c_str(), true), "Cannot configure detection fixture");
        std::stop_source stop;
        std::atomic<bool> childStarted = false;
        const auto started = SDL_GetTicks();
        {
            std::jthread canceller([&](std::stop_token token) {
                while (!token.stop_requested() && SDL_GetTicks() - started < 5000) {
                    if (std::filesystem::exists(folder / "started")) { childStarted = true; break; }
                    SDL_Delay(5);
                }
                stop.request_stop();
            });
            detectionCancelled = false;
            try { (void)musdesk::detectMediaTools(stop.get_token()); }
            catch (const std::exception&) { detectionCancelled = true; }
        }
        if (previous) SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "MUSDESK_FFMPEG_DIR", previous->c_str(), true);
        else SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "MUSDESK_FFMPEG_DIR");
        require(childStarted && detectionCancelled && SDL_GetTicks() - started < 5000,
            "Cancellation must terminate a running tool without waiting for its timeout");
#endif
        require(musdesk::isAudioPath("track.MP3") && musdesk::isAudioPath("track.wav")
            && musdesk::isAudioPath("track.ogg") && !musdesk::isAudioPath("track.png"), "Audio extension checks failed");
        bool rejected = false;
        try { (void)musdesk::importAudio("missing.mp3", {}); }
        catch (const std::exception& error) {
            rejected = std::string(error.what()).find("ffprobe") != std::string::npos;
        }
        require(rejected, "Missing ffprobe must produce an actionable error");
        if (argc > 1) {
            const auto tools = musdesk::detectMediaTools();
            std::cout << tools.status << '\n';
            require(!tools.ffmpegPath.empty() && !tools.ffprobePath.empty(), "Both media tools must be detected for integration checks");
            for (int i = 1; i < argc; ++i) {
                const auto audio = musdesk::importAudio(argv[i], tools);
                require(std::isfinite(audio.durationSeconds) && audio.durationSeconds > 0, "Positive duration expected");
                std::cout << "Audio duration: " << audio.durationSeconds << " seconds\n";
                const auto decoded = musdesk::prepareAudio(argv[i], tools);
                std::ofstream report(std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures"
                    / ("rhythm-levels-"+std::to_string(i)+".csv"));
                report << "band,time_seconds,level_db,accepted_minus27\n";
                for (int index = 0; index < 3; ++index) {
                    const auto band = static_cast<musdesk::BeatBand>(index);
                    const auto& events = decoded.rhythm.events(band);
                    require(std::is_sorted(events.begin(),events.end(),[](const auto& a,const auto& b) {
                        return a.timeSeconds < b.timeSeconds;
                    }),"Attack cache must remain sorted for deterministic seeking");
                    std::size_t previousCount = events.size();
                    std::cout << "Band " << index << " candidates " << events.size();
                    for (double db : {-45.0,-27.0,-20.0,-12.0,0.0}) {
                        const auto gate = musdesk::beatLevelFromDb(db);
                        std::size_t accepted = 0;
                        for (const auto& event : events) {
                            if (event.strength < gate) continue;
                            ++accepted;
                            const auto reaction = decoded.reactionAt(event.timeSeconds,gate,band);
                            require(reaction.beat == event.strength && std::abs(reaction.kick) == event.strength,
                                "Pulse and Side Kick must accept the same level-gated attacks");
                        }
                        require(accepted <= previousCount,"Raising the level gate must never accept more attacks");
                        previousCount = accepted;
                        std::cout << " gate " << db << " dB=" << accepted;
                    }
                    std::cout << '\n';
                    for (const auto& event : events)
                        report << index << ',' << event.timeSeconds << ',' << musdesk::beatLevelDb(event.strength)
                            << ',' << (event.strength >= musdesk::beatLevelFromDb(-27)) << '\n';
                }
                require(!decoded.samples.empty() && std::abs(decoded.durationSeconds() - audio.durationSeconds) < 1,
                    "Decoded PCM duration must agree with container metadata within codec padding");
                require(!decoded.envelope.values.empty(), "Decoded audio must include the RMS envelope");
                require(!decoded.waveform.peaks.empty() && decoded.waveform.peaks.size() <= 8192,
                    "Decoded audio must include a bounded waveform");
                require(!decoded.rhythm.bass.values.empty()
                    && decoded.rhythm.bass.durationSeconds == decoded.durationSeconds(),
                    "Decoded bass analysis must cover the audio timeline");
                std::cout << "Bass onsets: " << decoded.rhythm.beats.size() << '\n';
                const auto energy = decoded.envelope.at(decoded.durationSeconds() / 2);
                require(std::isfinite(energy) && energy >= 0 && energy <= 1, "Invalid decoded envelope");
                std::cout << "Decoded duration: " << decoded.durationSeconds() << " seconds; RMS windows: " << decoded.envelope.values.size() << '\n';
            }
            rejected = false;
            try { (void)musdesk::importAudio("nonexistent-audio-file.mp3", tools); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "Missing audio file must be rejected");
            const auto brokenPath = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures" / "broken.wav";
            std::filesystem::create_directories(brokenPath.parent_path());
            { std::ofstream broken(brokenPath, std::ios::binary); broken << "not audio"; }
            const auto bytes = brokenPath.u8string();
            rejected = false;
            try { (void)musdesk::importAudio(std::string(bytes.begin(), bytes.end()), tools); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "Damaged audio file must be rejected");
        }
        std::cout << "Audio import checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
