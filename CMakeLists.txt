cmake_minimum_required(VERSION 3.20)
project(normalmap_converter CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
set(BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    DirectXTex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG        jul2025
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(DirectXTex)
set_target_properties(DirectXTex PROPERTIES SYSTEM TRUE)
target_compile_options(DirectXTex PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wno-padded>)

add_executable(converter main.cpp)
target_link_libraries(converter PRIVATE DirectXTex)
