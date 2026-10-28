#include "musdesk/projects/RecoveryStore.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
template<class Action> void rejected(Action action) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Unsafe or invalid recovery operation was accepted");
}
void crashFixture(const std::string& root) {
    const auto executable = utf8(std::filesystem::u8path(SDL_GetBasePath()) / "MusDeskRecoveryTests.exe");
#ifdef _WIN32
    const auto program = executable;
#else
    const auto program = utf8(std::filesystem::u8path(SDL_GetBasePath()) / "MusDeskRecoveryTests");
#endif
    const char* args[] = {program.c_str(), "--crash-fixture", root.c_str(), nullptr};
    auto* process = SDL_CreateProcess(args, false);
    require(process != nullptr, "Could not start crash-recovery fixture");
    const auto deadline = SDL_GetTicks() + 10000;
    int code = -1;
    while (!SDL_WaitProcess(process, false, &code)) {
        if (SDL_GetTicks() > deadline) {
            SDL_KillProcess(process, true);
            SDL_WaitProcess(process, true, nullptr);
            SDL_DestroyProcess(process);
            throw std::runtime_error("Crash-recovery fixture timed out");
        }
        SDL_Delay(5);
    }
    SDL_DestroyProcess(process);
    require(code == 0, "Crash-recovery fixture failed");
}
}

int main(int argc, char* argv[]) {
    if (argc == 3 && std::string(argv[1]) == "--crash-fixture") {
        musdesk::RecoveryStore store(argv[2]);
        musdesk::ProjectDocument document;
        document.composition.title = "Abrupt exit recovery";
        document.previewTimeSeconds = 4.5;
        store.save(document);
        std::_Exit(0);
    }
    try {
        const auto root = std::filesystem::u8path(SDL_GetBasePath()) / "test-fixtures"
            / ("recovery-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        const auto copies = root / "copies";
        std::filesystem::create_directories(root);
        const auto manual = utf8(root / "manual.musdesk");
        musdesk::ProjectDocument saved;
        musdesk::saveProject(manual, saved);
        musdesk::ProjectDocument edited = saved;
        edited.composition.title = "Unsaved edit";
        edited.audioPath = utf8(root / "missing.wav");
        edited.previewTimeSeconds = 12.5;
        musdesk::RecoveryStore first(utf8(copies));
        first.save(edited);
        require(first.available().empty(), "An active session must not be offered for recovery");
        require(musdesk::loadProject(first.snapshotPath()) == edited, "Recovery snapshot must preserve project and media paths");
        require(musdesk::loadProject(manual) == saved, "Autosave must never overwrite the manually saved project");
        std::string secondPath;
        {
            musdesk::RecoveryStore second(utf8(copies));
            secondPath = second.snapshotPath();
            require(secondPath != first.snapshotPath(), "Sessions need separate recovery slots");
            second.save(saved);
            require(second.available().empty(), "Other active sessions must be excluded");
            rejected([&] { (void)second.restore(first.snapshotPath()); });
            rejected([&] { second.discard(first.snapshotPath()); });
            second.clear();
            require(std::filesystem::exists(std::filesystem::u8path(first.snapshotPath())), "Cleanup removed another session's snapshot");
            second.save(saved);
        }
        auto available = first.available();
        require(available.size() == 1 && available[0].path == secondPath, "Inactive session snapshot was not discovered");
        require(first.restore(secondPath) == saved && available[0].error.empty() && !available[0].savedAt.empty(), "Inactive recovery could not be read");
        first.discard(secondPath);
        require(first.available().empty(), "Discard did not remove the selected inactive copy");
        auto invalid = edited;
        invalid.composition.preset.glowIntensity = std::numeric_limits<float>::quiet_NaN();
        rejected([&] { first.save(invalid); });
        require(musdesk::loadProject(first.snapshotPath()) == edited, "Failed autosave destroyed the previous good copy");
        rejected([&] { (void)first.restore(manual); });
        rejected([&] { first.discard(manual); });
        require(musdesk::loadProject(manual) == saved, "Recovery cleanup changed a manual project");
        const auto broken = copies / "session-corrupt" / "snapshot.musdesk";
        std::filesystem::create_directories(broken.parent_path());
        { std::ofstream file(broken); file << "{"; }
        available = first.available();
        require(available.size() == 1 && !available[0].error.empty(), "Corrupt recovery must be reported without blocking startup");
        rejected([&] { (void)first.restore(utf8(broken)); });
        require(std::filesystem::exists(broken), "Failed recovery must retain its source copy");
        first.discard(utf8(broken));
        crashFixture(utf8(copies));
        available = first.available();
        require(available.size() == 1 && available[0].title == "Abrupt exit recovery", "Recovery after abrupt process exit failed");
        require(first.restore(available[0].path).previewTimeSeconds == 4.5, "Recovered playhead position was lost");
        first.clear();
        require(first.available().size() == 1, "Clearing the current session removed another recovery copy");
        rejected([&] { musdesk::RecoveryStore unavailable(manual); });
        require(musdesk::loadProject(manual) == saved, "Storage initialization failure changed the project");
        std::cout << "Recovery after abrupt exit, session locks, paths, invalid snapshots and safe cleanup passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
