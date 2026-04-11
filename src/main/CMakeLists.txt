file(
    GLOB_RECURSE MASODIK_CORE_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/api/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/config/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/diagnostics/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/genai/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/http/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/ovms/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/runtime/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/server/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/split/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/tui/*.cpp"
)

if(MASODIK_CORE_SOURCES)
    add_library(masodik-core STATIC ${MASODIK_CORE_SOURCES})
    add_library(masodik::core ALIAS masodik-core)
    target_include_directories(masodik-core PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(masodik-core PUBLIC masodik::openvino winhttp)
    if(TARGET ftxui::component)
        target_link_libraries(masodik-core PUBLIC ftxui::component ftxui::dom ftxui::screen)
    endif()
endif()

set(MASODIK_LAUNCHER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/launcher_app.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tui_app.cpp")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/split_artifact_tool.cpp")
    set(MASODIK_SPLIT_ARTIFACT_TOOL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/split_artifact_tool.cpp")
endif()

if(MASODIK_LAUNCHER_SOURCES)
    add_executable(masodik-launcher ${MASODIK_LAUNCHER_SOURCES})
    target_include_directories(masodik-launcher PRIVATE "${PROJECT_SOURCE_DIR}/include")
    if(TARGET masodik::core)
        target_link_libraries(masodik-launcher PRIVATE masodik::core)
    else()
        target_link_libraries(masodik-launcher PRIVATE masodik::openvino)
    endif()
    masodik_deploy_runtime_dependencies(masodik-launcher)
else()
    add_custom_target(
        masodik-launcher-bootstrap
        COMMENT "A launcher forrasfajljai meg nincsenek letrehozva."
    )
endif()

if(MASODIK_SPLIT_ARTIFACT_TOOL_SOURCE)
    add_executable(masodik-split-artifact-tool "${MASODIK_SPLIT_ARTIFACT_TOOL_SOURCE}")
    target_include_directories(masodik-split-artifact-tool PRIVATE "${PROJECT_SOURCE_DIR}/include")
    if(TARGET masodik::core)
        target_link_libraries(masodik-split-artifact-tool PRIVATE masodik::core)
    else()
        target_link_libraries(masodik-split-artifact-tool PRIVATE masodik::openvino)
    endif()
    masodik_deploy_runtime_dependencies(masodik-split-artifact-tool)
endif()

