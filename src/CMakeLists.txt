# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# minimp3 is a small, public-domain MP3 decoder. Fetch its pinned single-header
# release during CMake configuration so the firmware source remains lightweight.
set(MINIMP3_DIR "${CMAKE_BINARY_DIR}/generated/minimp3")
set(MINIMP3_HEADER "${MINIMP3_DIR}/minimp3.h")
file(MAKE_DIRECTORY "${MINIMP3_DIR}")
if(NOT EXISTS "${MINIMP3_HEADER}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h"
        "${MINIMP3_HEADER}"
        EXPECTED_HASH "SHA256=5fb296a790734772b65a5514438cc06d14f24e539c13475ff2a7f737681c82c0"
        TLS_VERIFY ON
        STATUS MINIMP3_DOWNLOAD_STATUS)
    list(GET MINIMP3_DOWNLOAD_STATUS 0 MINIMP3_DOWNLOAD_RESULT)
    if(NOT MINIMP3_DOWNLOAD_RESULT EQUAL 0)
        message(FATAL_ERROR "Unable to download minimp3: ${MINIMP3_DOWNLOAD_STATUS}")
    endif()
endif()

idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." "${MINIMP3_DIR}")
