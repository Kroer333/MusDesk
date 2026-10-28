#pragma once

#include "musdesk/projects/ProjectStore.hpp"
#include <filesystem>
#include <memory>
#include <vector>

namespace musdesk {
struct RecoveryCopy {
    std::string path;
    std::string title;
    std::string savedAt;
    std::string error;
};

std::string defaultRecoveryDirectory();

class RecoveryStore {
public:
    explicit RecoveryStore(const std::string& directory);
    ~RecoveryStore();
    RecoveryStore(const RecoveryStore&) = delete;
    RecoveryStore& operator=(const RecoveryStore&) = delete;
    void save(const ProjectDocument& project);
    void clear();
    std::vector<RecoveryCopy> available() const;
    ProjectDocument restore(const std::string& path) const;
    void discard(const std::string& path) const;
    std::string snapshotPath() const;

private:
    struct Lock;
    std::filesystem::path checkedCopy(const std::string& path) const;
    std::filesystem::path root_;
    std::filesystem::path session_;
    std::unique_ptr<Lock> lock_;
};
}
