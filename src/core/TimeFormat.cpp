#include "musdesk/core/TimeFormat.hpp"

#include <iomanip>
#include <sstream>

namespace musdesk {
std::string formatTime(std::uint64_t seconds) {
    const std::uint64_t hours = seconds / 3600;
    const std::uint64_t minutes = (seconds / 60) % 60;
    const std::uint64_t remainingSeconds = seconds % 60;

    std::ostringstream result;
    result << std::setfill('0');
    if (hours > 0) {
        result << std::setw(2) << hours << ':';
        result << std::setw(2) << minutes;
    } else {
        result << std::setw(2) << (seconds / 60);
    }
    result << ':' << std::setw(2) << remainingSeconds;
    return result.str();
}
}
