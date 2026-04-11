set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x64)

if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(
        CMAKE_CXX_COMPILER
        "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64/cl.exe"
        CACHE FILEPATH "MSVC C++ compiler"
    )
endif()

if(NOT DEFINED CMAKE_C_COMPILER)
    set(
        CMAKE_C_COMPILER
        "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64/cl.exe"
        CACHE FILEPATH "MSVC C compiler"
    )
endif()

set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "Generator platform")

