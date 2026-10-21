#include "musdesk/core/TimeFormat.hpp"

#include <iostream>

int main() {
    std::cout << "MusDesk bootstrap (not yet a visualizer)\n";
    std::cout << "Example track position: " << musdesk::formatTime(125) << '\n';
    return 0;
}
