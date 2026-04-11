cmake_minimum_required(VERSION 3.26)

project(
    masodik
    VERSION 0.1.0
    DESCRIPTION "Helyi hibrid AI kiszolgalas C++23 alapon"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MASODIK_ENABLE_OPENVINO "OpenVINO integracio engedelyezese" ON)
option(MASODIK_ENABLE_TESTS "Tesztek engedelyezese" ON)
option(MASODIK_ENABLE_BENCHMARKS "Benchmarkok engedelyezese" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies")

include(OpenVINO)
include(CTest)

# FTXUI — terminal felhasznaloi felület a TUI chat interfeszhez
include(FetchContent)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main/CMakeLists.txt")
    add_subdirectory(src/main)
endif()

if(MASODIK_ENABLE_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

if(MASODIK_ENABLE_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    add_subdirectory(benchmarks)
endif()

