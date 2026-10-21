#pragma once

#include "musdesk/core/VisualPreset.hpp"

#include <string_view>

namespace musdesk {
VisualPreset parsePreset(std::string_view json);
std::string serializePreset(const VisualPreset& preset);
VisualPreset loadPreset(const std::string& path);
void savePreset(const std::string& path, const VisualPreset& preset);
}
