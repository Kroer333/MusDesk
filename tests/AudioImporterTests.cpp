#include "musdesk/media/AudioImporter.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char* argv[]) {
    try {
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
                require(!decoded.samples.empty() && std::abs(decoded.durationSeconds() - audio.durationSeconds) < 1,
                    "Decoded PCM duration must agree with container metadata within codec padding");
                require(!decoded.envelope.values.empty(), "Decoded audio must include the RMS envelope");
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
