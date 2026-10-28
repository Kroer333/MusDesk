#pragma once

#include "musdesk/core/Composition.hpp"
#include <string_view>

namespace musdesk {
struct ProjectDocument {
    CompositionSettings composition;
    std::string audioPath;
    std::string artworkPath;
    double previewTimeSeconds = 0;
    bool operator==(const ProjectDocument&) const = default;
};

bool isProjectPath(const std::string& path);
ProjectDocument parseProject(std::string_view text);
std::string serializeProject(const ProjectDocument& project);
ProjectDocument loadProject(const std::string& path);
void saveProject(const std::string& path, const ProjectDocument& project);
}
