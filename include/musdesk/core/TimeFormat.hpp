#pragma once

#include <cstdint>
#include <string>

namespace musdesk {
// Formats seconds as MM:SS, switching to HH:MM:SS for tracks >= one hour.
std::string formatTime(std::uint64_t seconds);
}
