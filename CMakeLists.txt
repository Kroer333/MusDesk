cmake_minimum_required(VERSION 3.21)
project(MusDesk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(SDL_SHARED ON CACHE BOOL "Build SDL as a shared library")
set(SDL_STATIC OFF CACHE BOOL "Do not build a second, static SDL library")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build SDL's test support library")
set(SDL_TESTS OFF CACHE BOOL "Do not build SDL's own tests")
set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL's examples")

FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
)
FetchContent_MakeAvailable(SDL3)

add_library(musdesk_core STATIC
    src/core/TimeFormat.cpp
)

target_include_directories(musdesk_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_executable(MusDesk src/main.cpp)
target_link_libraries(MusDesk PRIVATE musdesk_core SDL3::SDL3)

if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET MusDesk POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3-shared>"
            "$<TARGET_FILE_DIR:MusDesk>"
        VERBATIM
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(MusDeskTimeTests tests/TimeFormatTests.cpp)
    target_link_libraries(MusDeskTimeTests PRIVATE musdesk_core)
    add_test(NAME MusDeskTimeTests COMMAND MusDeskTimeTests)
endif()
