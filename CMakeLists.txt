cmake_minimum_required(VERSION 3.20)
project(normalmap_converter CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --- CPM bootstrap ---
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
    message(STATUS "Downloading CPM.cmake...")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake"
        "${CPM_DOWNLOAD_LOCATION}"
    )
endif()
include("${CPM_DOWNLOAD_LOCATION}")

# --- DirectXTex ---
set(BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)

CPMAddPackage(
    NAME DirectXTex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG        jul2025
    GIT_SHALLOW    TRUE
)
set_target_properties(DirectXTex PROPERTIES SYSTEM TRUE)
target_compile_options(DirectXTex PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wno-padded>)

CPMAddPackage(
    NAME slang_bin
    URL          https://github.com/shader-slang/slang/releases/download/v2026.7/slang-2026.7-windows-x86_64.zip
    DOWNLOAD_ONLY YES
)
set(slang_DIR "${slang_bin_SOURCE_DIR}/cmake")
find_package(slang CONFIG REQUIRED)

add_executable(converter main.cpp slang_mips.cpp)
target_link_libraries(converter PRIVATE DirectXTex slang::slang slang::gfx)

# output to release/
set_target_properties(converter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/release"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/release"
)

add_custom_command(TARGET converter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:texdiag>"
        "${CMAKE_SOURCE_DIR}/release/texdiag.exe"
)

add_custom_command(TARGET converter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${slang_bin_SOURCE_DIR}/bin"
        "$<TARGET_FILE_DIR:converter>"
)

add_custom_command(TARGET converter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:converter>/shaders"
)
