cmake_minimum_required(VERSION 3.21)
project(MusDesk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(musdesk_core STATIC
    src/core/TimeFormat.cpp
)

target_include_directories(musdesk_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_executable(MusDesk src/main.cpp)
target_link_libraries(MusDesk PRIVATE musdesk_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(MusDeskTimeTests tests/TimeFormatTests.cpp)
    target_link_libraries(MusDeskTimeTests PRIVATE musdesk_core)
    add_test(NAME MusDeskTimeTests COMMAND MusDeskTimeTests)
endif()
