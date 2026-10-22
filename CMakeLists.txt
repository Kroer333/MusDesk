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

find_package(OpenGL REQUIRED)

FetchContent_Declare(imgui
    URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.93.0
    URL_HASH SHA256=f7bf6bc76e5d79d40d751096663e6c5d18daf08129dd52e7f7b09e4251c8eeee
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(stb
    URL https://codeload.github.com/nothings/stb/tar.gz/a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d
    URL_HASH SHA256=ae498e10fadeba0c2e7aa86c71cc9989c4d895201cd26b8b72061c76cfde4edf
)
FetchContent_MakeAvailable(stb)

set(JSON_BuildTests OFF CACHE BOOL "Do not build dependency tests")
set(JSON_Install OFF CACHE BOOL "Do not install JSON separately")
FetchContent_Declare(nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
)
FetchContent_MakeAvailable(nlohmann_json)

foreach(option IN ITEMS ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO
        LIBWEBPMUX WEBPMUX EXTRAS WEBP_JS FUZZTEST)
    set(WEBP_BUILD_${option} OFF CACHE BOOL "Disable unused WebP tools")
endforeach()
set(WEBP_USE_THREAD OFF CACHE BOOL "Decode artwork on the importing thread")
FetchContent_Declare(libwebp
    URL https://codeload.github.com/webmproject/libwebp/tar.gz/4fa21912338357f89e4fd51cf2368325b59e9bd9
    URL_HASH SHA256=923f3382a47a2af185c3240c954cf004428b237bd7317413a95146d01eb4b94b
)
FetchContent_MakeAvailable(libwebp)
# Pull in only webpdecoder and its dependencies, not the unused encoder libraries.
set_property(DIRECTORY "${libwebp_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

# Dear ImGui provides source files; build its core and the two chosen backends.
add_library(musdesk_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(musdesk_imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)
target_link_libraries(musdesk_imgui PRIVATE SDL3::SDL3 OpenGL::GL)

add_library(musdesk_core STATIC
    src/core/TimeFormat.cpp
    src/core/Composition.cpp
    src/core/AudioAnalysis.cpp
    src/core/VisualPreset.cpp
)

target_include_directories(musdesk_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(musdesk_media STATIC src/media/ImageLoader.cpp src/media/AudioImporter.cpp)
target_include_directories(musdesk_media PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(musdesk_media SYSTEM PRIVATE "${stb_SOURCE_DIR}")
target_link_libraries(musdesk_media PUBLIC musdesk_core PRIVATE SDL3::SDL3 webpdecoder)

add_library(musdesk_audio STATIC src/audio/AudioPlayer.cpp)
target_include_directories(musdesk_audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(musdesk_audio PUBLIC musdesk_media PRIVATE SDL3::SDL3)

add_library(musdesk_presets STATIC src/presets/PresetStore.cpp)
target_include_directories(musdesk_presets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(musdesk_presets PUBLIC musdesk_core PRIVATE nlohmann_json::nlohmann_json SDL3::SDL3)

add_library(musdesk_render STATIC src/render/VisualizerRenderer.cpp)
target_include_directories(musdesk_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(musdesk_render SYSTEM PRIVATE "${stb_SOURCE_DIR}")
target_link_libraries(musdesk_render PUBLIC musdesk_core musdesk_media PRIVATE SDL3::SDL3 OpenGL::GL)

add_library(musdesk_export STATIC src/export/ExportEngine.cpp)
target_include_directories(musdesk_export PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(musdesk_export PUBLIC musdesk_render PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json)

add_executable(MusDesk
    src/main.cpp
    src/app/EditorUi.cpp
    src/app/ArtworkDialog.cpp
)
target_link_libraries(MusDesk PRIVATE musdesk_export musdesk_audio musdesk_presets musdesk_imgui SDL3::SDL3 OpenGL::GL)
target_compile_definitions(MusDesk PRIVATE MUSDESK_VERSION="${PROJECT_VERSION}")

function(musdesk_copy_runtime_files target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>/assets"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf"
            "$<TARGET_FILE_DIR:${target}>/assets/Roboto-Medium.ttf"
        VERBATIM
    )
    if(WIN32 AND TARGET SDL3::SDL3-shared)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3-shared>"
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM
        )
    endif()
endfunction()
musdesk_copy_runtime_files(MusDesk)
include(cmake/MediaTools.cmake)
musdesk_stage_media_tools(MusDesk)

include(CTest)
if(BUILD_TESTING)
    add_executable(MusDeskExportTests tests/ExportTests.cpp)
    target_link_libraries(MusDeskExportTests PRIVATE musdesk_export SDL3::SDL3 OpenGL::GL)
    musdesk_copy_runtime_files(MusDeskExportTests)
    if(TARGET MusDeskMediaTools)
        add_dependencies(MusDeskExportTests MusDeskMediaTools)
    endif()
    add_test(NAME MusDeskExportTimelineTests COMMAND MusDeskExportTests --timeline)
    add_test(NAME MusDeskVersion COMMAND MusDesk --version)
    set_tests_properties(MusDeskVersion PROPERTIES PASS_REGULAR_EXPRESSION "MusDesk ${PROJECT_VERSION}")
    add_test(NAME MusDeskInvalidArguments COMMAND MusDesk --invalid-option)
    set_tests_properties(MusDeskInvalidArguments PROPERTIES WILL_FAIL TRUE)
    add_executable(MusDeskPresetTests tests/PresetTests.cpp)
    target_link_libraries(MusDeskPresetTests PRIVATE musdesk_presets SDL3::SDL3)
    musdesk_copy_runtime_files(MusDeskPresetTests)
    add_test(NAME MusDeskPresetTests COMMAND MusDeskPresetTests "${CMAKE_CURRENT_SOURCE_DIR}/presets")
    add_executable(MusDeskAnalysisTests tests/AudioAnalysisTests.cpp)
    target_link_libraries(MusDeskAnalysisTests PRIVATE musdesk_core)
    add_test(NAME MusDeskAnalysisTests COMMAND MusDeskAnalysisTests)

    add_executable(MusDeskPlaybackTests tests/AudioPlayerTests.cpp)
    target_link_libraries(MusDeskPlaybackTests PRIVATE musdesk_audio SDL3::SDL3)
    musdesk_copy_runtime_files(MusDeskPlaybackTests)
    add_test(NAME MusDeskPlaybackTests COMMAND MusDeskPlaybackTests)
    set_tests_properties(MusDeskPlaybackTests PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")

    add_executable(MusDeskAudioTests tests/AudioImporterTests.cpp)
    if(TARGET MusDeskMediaTools)
        add_dependencies(MusDeskAudioTests MusDeskMediaTools)
    endif()
    target_link_libraries(MusDeskAudioTests PRIVATE musdesk_media SDL3::SDL3)
    musdesk_copy_runtime_files(MusDeskAudioTests)
    add_test(NAME MusDeskAudioTests COMMAND MusDeskAudioTests)
    add_executable(MusDeskTimeTests tests/TimeFormatTests.cpp)
    target_link_libraries(MusDeskTimeTests PRIVATE musdesk_core)
    add_test(NAME MusDeskTimeTests COMMAND MusDeskTimeTests)

    add_executable(MusDeskCompositionTests tests/CompositionTests.cpp)
    target_link_libraries(MusDeskCompositionTests PRIVATE musdesk_core)
    add_test(NAME MusDeskCompositionTests COMMAND MusDeskCompositionTests)

    add_executable(MusDeskImageTests tests/ImageLoaderTests.cpp)
    target_include_directories(MusDeskImageTests SYSTEM PRIVATE "${stb_SOURCE_DIR}")
    target_link_libraries(MusDeskImageTests PRIVATE musdesk_media SDL3::SDL3)
    musdesk_copy_runtime_files(MusDeskImageTests)
    add_custom_command(TARGET MusDeskImageTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:MusDeskImageTests>/test-fixtures"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${libwebp_SOURCE_DIR}/examples/test.webp"
            "$<TARGET_FILE_DIR:MusDeskImageTests>/test-fixtures/codec-sample.webp"
        VERBATIM
    )
    add_test(NAME MusDeskImageTests COMMAND MusDeskImageTests)

    add_executable(MusDeskRenderTests tests/RenderTests.cpp)
    target_link_libraries(MusDeskRenderTests PRIVATE musdesk_render musdesk_presets SDL3::SDL3 OpenGL::GL)
    musdesk_copy_runtime_files(MusDeskRenderTests)
    option(MUSDESK_ENABLE_RENDER_TESTS "Register tests requiring an OpenGL 3.3 graphics driver" OFF)
    if(MUSDESK_ENABLE_RENDER_TESTS)
        add_test(NAME MusDeskRenderTests COMMAND MusDeskRenderTests)
        add_test(NAME MusDeskExportTests COMMAND MusDeskExportTests)
        set_tests_properties(MusDeskExportTests PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()
    if(MSVC)
        target_compile_options(MusDeskImageTests PRIVATE /utf-8)
        target_compile_options(MusDeskPresetTests PRIVATE /utf-8)
    endif()
endif()

include(cmake/Packaging.cmake)
