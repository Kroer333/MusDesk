#pragma once

#include "musdesk/projects/ProjectStore.hpp"
#include <stop_token>

namespace musdesk {
std::string collectProject(const std::string& parentDirectory, const ProjectDocument& project,
    std::stop_token cancellation = {});
}
