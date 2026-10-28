#include "musdesk/projects/RecoveryStore.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace musdesk {
namespace {
std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
std::string timestamp(std::filesystem::file_time_type time) {
    const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(time);
    const auto seconds = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(systemTime));
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    std::ostringstream text;
    text << std::put_time(&utc, "%Y-%m-%d %H:%M:%S UTC");
    return text.str();
}
}

struct RecoveryStore::Lock {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    Lock(const std::filesystem::path& path, bool create) {
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            create ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    ~Lock() { if (valid()) CloseHandle(handle); }
    bool valid() const { return handle != INVALID_HANDLE_VALUE; }
#else
    int descriptor = -1;
    Lock(const std::filesystem::path& path, bool create) {
        descriptor = open(path.c_str(), O_RDWR | (create ? O_CREAT | O_EXCL : 0), 0600);
        if (descriptor >= 0 && flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
            close(descriptor);
            descriptor = -1;
        }
    }
    ~Lock() { if (valid()) close(descriptor); }
    bool valid() const { return descriptor >= 0; }
#endif
};

std::string defaultRecoveryDirectory() {
    if (const auto* configured = SDL_getenv("MUSDESK_RECOVERY_DIR"); configured && *configured) return configured;
    std::unique_ptr<char, decltype(&SDL_free)> preferences(SDL_GetPrefPath("", "MusDesk"), SDL_free);
    if (!preferences) throw std::runtime_error(std::string("Cannot locate recovery storage: ") + SDL_GetError());
    return utf8(std::filesystem::u8path(preferences.get()) / "recovery");
}

RecoveryStore::RecoveryStore(const std::string& directory) {
    if (directory.empty()) throw std::runtime_error("Recovery directory is empty.");
    std::filesystem::create_directories(std::filesystem::u8path(directory));
    root_ = std::filesystem::canonical(std::filesystem::u8path(directory));
    static std::atomic<unsigned> counter{0};
    bool created = false;
    for (int attempt = 0; attempt < 16 && !created; ++attempt) {
        session_ = root_ / ("session-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
            + "-" + std::to_string(counter++));
        created = std::filesystem::create_directory(session_);
    }
    if (!created) throw std::runtime_error("Cannot reserve a recovery session directory.");
    lock_ = std::make_unique<Lock>(session_ / "session.lock", true);
    if (!lock_->valid()) {
        std::error_code error;
        std::filesystem::remove(session_, error);
        throw std::runtime_error("Cannot lock recovery storage for this session.");
    }
}

RecoveryStore::~RecoveryStore() {
    lock_.reset();
    std::error_code error;
    std::filesystem::remove(session_ / "session.lock", error);
    std::filesystem::remove(session_, error);
}

std::string RecoveryStore::snapshotPath() const { return utf8(session_ / "snapshot.musdesk"); }
void RecoveryStore::save(const ProjectDocument& project) { saveProject(snapshotPath(), project); }
void RecoveryStore::clear() { std::filesystem::remove(session_ / "snapshot.musdesk"); }

std::filesystem::path RecoveryStore::checkedCopy(const std::string& path) const {
    const auto file = std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal();
    const auto parent = file.parent_path();
    if (file.filename() != "snapshot.musdesk" || parent.parent_path() != root_
        || !utf8(parent.filename()).starts_with("session-")
        || std::filesystem::is_symlink(parent) || std::filesystem::is_symlink(file)
        || std::filesystem::canonical(parent) != parent)
        throw std::runtime_error("Recovery path is outside the recovery store.");
    const auto lockPath = parent / "session.lock";
    if (std::filesystem::exists(lockPath) && !Lock(lockPath, false).valid())
        throw std::runtime_error("This recovery copy belongs to an active or inaccessible session.");
    return file;
}

ProjectDocument RecoveryStore::restore(const std::string& path) const { return loadProject(utf8(checkedCopy(path))); }

void RecoveryStore::discard(const std::string& path) const {
    const auto file = checkedCopy(path);
    std::filesystem::remove(file);
    std::error_code error;
    std::filesystem::remove(file.parent_path() / "session.lock", error);
    std::filesystem::remove(file.parent_path(), error);
}

std::vector<RecoveryCopy> RecoveryStore::available() const {
    std::vector<std::pair<std::filesystem::file_time_type, std::string>> candidates;
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        std::error_code error;
        if (!entry.is_directory(error) || entry.is_symlink(error)) continue;
        const auto file = entry.path() / "snapshot.musdesk";
        if (!std::filesystem::is_regular_file(file, error)) continue;
        try {
            const auto checked = checkedCopy(utf8(file));
            candidates.emplace_back(std::filesystem::last_write_time(checked), utf8(checked));
        } catch (const std::exception&) { continue; }
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<RecoveryCopy> result;
    for (std::size_t i = 0; i < std::min<std::size_t>(20, candidates.size()); ++i) {
        RecoveryCopy copy{candidates[i].second, "Unreadable recovery copy", timestamp(candidates[i].first), {}};
        try {
            const auto document = loadProject(copy.path);
            copy.title = document.composition.title.empty() ? "Untitled project" : document.composition.title;
        } catch (const std::exception& error) { copy.error = error.what(); }
        result.push_back(std::move(copy));
    }
    return result;
}
}
