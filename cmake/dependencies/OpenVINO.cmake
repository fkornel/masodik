if(TARGET masodik_openvino)
    return()
endif()

add_library(masodik_openvino INTERFACE)
add_library(masodik::openvino ALIAS masodik_openvino)

function(masodik_deploy_runtime_dependencies target_name)
    if(TARGET "${target_name}")
        set(_masodik_runtime_files)
        if(EXISTS "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/bin/intel64/Debug")
            file(GLOB _masodik_openvino_debug_runtime_files
                "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/bin/intel64/Debug/*.dll")
            list(APPEND _masodik_runtime_files ${_masodik_openvino_debug_runtime_files})
        endif()
        if(EXISTS "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/bin/intel64/Release")
            file(GLOB _masodik_openvino_release_runtime_files
                "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/bin/intel64/Release/*.dll")
            list(APPEND _masodik_runtime_files ${_masodik_openvino_release_runtime_files})
        endif()
        if(EXISTS "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/3rdparty/tbb/bin")
            file(GLOB _masodik_tbb_runtime_files
                "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/3rdparty/tbb/bin/*.dll")
            list(APPEND _masodik_runtime_files ${_masodik_tbb_runtime_files})
        endif()

        add_custom_command(
            TARGET "${target_name}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:${target_name}> $<TARGET_FILE_DIR:${target_name}>
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${_masodik_runtime_files} $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

if(MASODIK_ENABLE_OPENVINO)
    if(EXISTS "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/cmake")
        set(_masodik_default_openvino_cmake_hint
            "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/cmake")
        set(_masodik_default_openvino_include_hint
            "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/include")
        set(_masodik_default_openvino_runtime_bin_hint
            "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/bin/intel64/Release")
        set(_masodik_default_openvino_genai_dll_hint
            "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/bin/intel64/Release/openvino_genai.dll")
        set(_masodik_default_openvino_genai_cmake_hint
            "${CMAKE_SOURCE_DIR}/openvino_genai_windows_2026.1.0.0_x86_64/runtime/cmake")
        set(_masodik_force_bundle_defaults TRUE)
    else()
        set(_masodik_default_openvino_cmake_hint
            "C:/Users/jezus/AppData/Local/Programs/AI Playground/resources/OpenVINO/.venv/Lib/site-packages/openvino/cmake")
        set(_masodik_default_openvino_include_hint
            "C:/Users/jezus/AppData/Local/Programs/AI Playground/resources/OpenVINO/.venv/Lib/site-packages/openvino/include")
        set(_masodik_default_openvino_runtime_bin_hint
            "C:/Users/jezus/AppData/Local/Programs/AI Playground/resources/OpenVINO/.venv/Lib/site-packages/openvino/libs")
        set(_masodik_default_openvino_genai_dll_hint
            "C:/Users/jezus/AppData/Local/Programs/AI Playground/resources/OpenVINO/ovms/openvino_genai.dll")
        set(_masodik_default_openvino_genai_cmake_hint
            "${CMAKE_SOURCE_DIR}/build/third_party/openvino-genai-install/runtime/cmake")
        set(_masodik_force_bundle_defaults FALSE)
    endif()

    if(_masodik_force_bundle_defaults)
        set(
            MASODIK_OPENVINO_CMAKE_HINT
            "${_masodik_default_openvino_cmake_hint}"
            CACHE PATH
            "OpenVINO CMake package hint"
            FORCE
        )
        set(
            MASODIK_OPENVINO_INCLUDE_HINT
            "${_masodik_default_openvino_include_hint}"
            CACHE PATH
            "OpenVINO fejlesztoi include konyvtar hint"
            FORCE
        )
        set(
            MASODIK_OPENVINO_RUNTIME_LIBS_HINT
            "${_masodik_default_openvino_runtime_bin_hint}"
            CACHE PATH
            "OpenVINO runtime DLL konyvtar hint"
            FORCE
        )
        set(
            MASODIK_OPENVINO_GENAI_DLL_HINT
            "${_masodik_default_openvino_genai_dll_hint}"
            CACHE FILEPATH
            "OpenVINO GenAI runtime DLL hint"
            FORCE
        )
        set(
            MASODIK_OPENVINO_GENAI_CMAKE_HINT
            "${_masodik_default_openvino_genai_cmake_hint}"
            CACHE PATH
            "Telepitett OpenVINO GenAI CMake package hint"
            FORCE
        )
    else()
        set(
            MASODIK_OPENVINO_CMAKE_HINT
            "${_masodik_default_openvino_cmake_hint}"
            CACHE PATH
            "OpenVINO CMake package hint"
        )
        set(
            MASODIK_OPENVINO_INCLUDE_HINT
            "${_masodik_default_openvino_include_hint}"
            CACHE PATH
            "OpenVINO fejlesztoi include konyvtar hint"
        )
        set(
            MASODIK_OPENVINO_RUNTIME_LIBS_HINT
            "${_masodik_default_openvino_runtime_bin_hint}"
            CACHE PATH
            "OpenVINO runtime DLL konyvtar hint"
        )
        set(
            MASODIK_OPENVINO_GENAI_DLL_HINT
            "${_masodik_default_openvino_genai_dll_hint}"
            CACHE FILEPATH
            "OpenVINO GenAI runtime DLL hint"
        )
        set(
            MASODIK_OPENVINO_GENAI_CMAKE_HINT
            "${_masodik_default_openvino_genai_cmake_hint}"
            CACHE PATH
            "Telepitett OpenVINO GenAI CMake package hint"
        )
    endif()

    set(OpenVINO_DIR "${MASODIK_OPENVINO_CMAKE_HINT}" CACHE PATH "OpenVINO CMake package dir" FORCE)

    # RelWithDebInfo és MinSizeRel fordítási konfigurációkat a Release importált
    # könyvtárhoz kell mappolni: az OpenVINO targets cmake csak DEBUG és RELEASE
    # konfigurációt definiál, CMake fallback nélkül Debug-ra esne RelWithDebInfo esetén.
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Release")
    set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "Release")

    find_package(OpenVINO CONFIG QUIET PATHS "${MASODIK_OPENVINO_CMAKE_HINT}" NO_DEFAULT_PATH)

    if(TARGET openvino::runtime)
        target_link_libraries(masodik_openvino INTERFACE openvino::runtime)
        if(EXISTS "${MASODIK_OPENVINO_INCLUDE_HINT}")
            target_include_directories(masodik_openvino INTERFACE "${MASODIK_OPENVINO_INCLUDE_HINT}")
        endif()
        target_compile_definitions(masodik_openvino INTERFACE MASODIK_HAS_OPENVINO=1)

        find_package(OpenVINOGenAI CONFIG QUIET PATHS "${MASODIK_OPENVINO_GENAI_CMAKE_HINT}" NO_DEFAULT_PATH)
        if(TARGET openvino::genai)
            target_link_libraries(masodik_openvino INTERFACE openvino::genai)
            target_compile_definitions(masodik_openvino INTERFACE MASODIK_HAS_OPENVINO_GENAI=1)
        else()
            target_compile_definitions(masodik_openvino INTERFACE MASODIK_HAS_OPENVINO_GENAI=0)
            message(STATUS "OpenVINO GenAI package nem talalhato meg, placeholder pipeline marad aktiv.")
        endif()
    else()
        target_compile_definitions(masodik_openvino INTERFACE MASODIK_HAS_OPENVINO=0)
        target_compile_definitions(masodik_openvino INTERFACE MASODIK_HAS_OPENVINO_GENAI=0)
        message(STATUS "OpenVINO nem talalhato meg, a build bootstrap modban marad.")
    endif()
else()
    target_compile_definitions(masodik_openvino INTERFACE MASODIK_HAS_OPENVINO=0)
    target_compile_definitions(masodik_openvino INTERFACE MASODIK_HAS_OPENVINO_GENAI=0)
endif()

