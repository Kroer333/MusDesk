#include "musdesk/audio/AudioPlayer.hpp"
#include "musdesk/media/AudioImporter.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Predicate> void eventually(Predicate predicate, const char* message) {
    const auto deadline = SDL_GetTicks() + 2000;
    while (!predicate()) {
        if (SDL_GetTicks() > deadline) throw std::runtime_error(message);
        SDL_Delay(5);
    }
}
}

int main(int, char*[]) {
    try {
        {
            auto clip = std::make_shared<musdesk::DecodedAudio>();
            clip->samples.resize(48000 * 2, 1000);
            musdesk::AudioPlayer player;
            player.load(clip);
            require(!player.state().playing && player.state().timeSeconds == 0, "Imported audio must start paused");
            player.setVolume(0.5f);
            player.play();
            eventually([&] { return player.state().timeSeconds > 0.04; }, "Playback clock must advance from consumed samples");
            player.pause();
            const auto paused = player.state().timeSeconds;
            SDL_Delay(30);
            require(player.state().timeSeconds == paused && !player.state().playing, "Pause must freeze the timeline");
            player.seek(0.4);
            require(std::abs(player.state().timeSeconds - 0.4) < 1.0 / 48000 && !player.state().playing,
                "Seeking while paused must remain paused and accurate to one sample frame");
            player.play();
            eventually([&] { return player.state().timeSeconds > 0.45; }, "Resume must continue from the seek target");
            player.seek(0.8);
            require(player.state().playing && player.state().timeSeconds >= 0.8, "Seeking while playing must preserve playback");
            eventually([&] { return player.state().ended; }, "Playback must reach the end");
            require(!player.state().playing && player.state().timeSeconds == 1, "End must stop at exact decoded duration");
            player.play();
            require(player.state().playing && player.state().timeSeconds < 0.2, "Play after end must restart");
            player.pause();
            player.seek(-10);
            require(player.state().timeSeconds == 0, "Negative seeks must clamp to zero");
            player.seek(20);
            require(player.state().ended && !player.state().playing, "Seeking beyond the end must clamp and pause");
            bool rejected = false;
            try { player.load({}); } catch (const std::exception&) { rejected = true; }
            require(rejected && player.loaded() && player.state().ended, "Invalid replacement must preserve the current clip");
            player.setVolume(0);
            player.unload();
            require(!player.loaded() && !player.state().playing, "Unloading must release playback and its previous clip");
            player.load(clip);
            player.seek(0.25);
            require(player.loaded() && !player.state().playing && std::abs(player.state().timeSeconds - 0.25) < 1.0 / 48000,
                "Reopening playback must support restoring the paused visual position");
        }
        SDL_Quit();
        std::cout << "Audio playback, pause, seeks, end, restart, gain and replacement checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
