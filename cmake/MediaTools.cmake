include_guard(GLOBAL)

set(MUSDESK_FFMPEG_DIR "" CACHE PATH "Directory containing ffmpeg and ffprobe")
find_program(MUSDESK_FFMPEG_EXECUTABLE NAMES ffmpeg
    HINTS "${MUSDESK_FFMPEG_DIR}" "$ENV{MUSDESK_FFMPEG_DIR}" "${PROJECT_SOURCE_DIR}/tools/ffmpeg/bin"
    NO_CACHE)
find_program(MUSDESK_FFPROBE_EXECUTABLE NAMES ffprobe
    HINTS "${MUSDESK_FFMPEG_DIR}" "$ENV{MUSDESK_FFMPEG_DIR}" "${PROJECT_SOURCE_DIR}/tools/ffmpeg/bin"
    NO_CACHE)

function(musdesk_stage_media_tools target)
    if(NOT MUSDESK_FFMPEG_EXECUTABLE OR NOT MUSDESK_FFPROBE_EXECUTABLE)
        message(WARNING "Audio tools not found. Put ffmpeg/ffprobe in tools/ffmpeg/bin, set MUSDESK_FFMPEG_DIR, or add them to PATH; then reconfigure CMake.")
        return()
    endif()
    add_custom_target(${target}MediaTools
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${MUSDESK_FFMPEG_EXECUTABLE}" "${MUSDESK_FFPROBE_EXECUTABLE}"
            "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
    add_dependencies(${target} ${target}MediaTools)
    message(STATUS "Audio tools: ${MUSDESK_FFMPEG_EXECUTABLE}; ${MUSDESK_FFPROBE_EXECUTABLE}")
endfunction()
