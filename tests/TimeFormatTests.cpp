#include "musdesk/core/TimeFormat.hpp"

#include <cstdint>
#include <iostream>
#include <string>

int main() {
    struct TestCase {
        std::uint64_t seconds;
        std::string expected;
    };

    const TestCase cases[] = {
        {0, "00:00"},
        {9, "00:09"},
        {59, "00:59"},
        {60, "01:00"},
        {125, "02:05"},
        {3599, "59:59"},
        {3600, "01:00:00"},
        {3661, "01:01:01"},
    };

    for (const auto& test : cases) {
        const std::string actual = musdesk::formatTime(test.seconds);
        if (actual != test.expected) {
            std::cerr << "Time formatting failed for " << test.seconds
                      << ": expected " << test.expected
                      << ", got " << actual << '\n';
            return 1;
        }
    }

    std::cout << "All time formatting tests passed.\n";
    return 0;
}
