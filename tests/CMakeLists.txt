file(GLOB_RECURSE MASODIK_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(MASODIK_TEST_SOURCES)
    add_executable(masodik-tests ${MASODIK_TEST_SOURCES})
    target_include_directories(masodik-tests PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/tests")
    if(TARGET masodik::core)
        target_link_libraries(masodik-tests PRIVATE masodik::core)
    else()
        target_link_libraries(masodik-tests PRIVATE masodik::openvino)
    endif()
    masodik_deploy_runtime_dependencies(masodik-tests)
    masodik_register_ctest(masodik-tests)
    add_custom_target(masodik-regression
        COMMAND "$<TARGET_FILE:masodik-tests>"
        DEPENDS masodik-tests
        USES_TERMINAL
        COMMENT "A teljes natív regressziós tesztkör futtatása.")
    add_custom_target(masodik-manual-split-tests
        COMMAND "$<TARGET_FILE:masodik-tests>"
        DEPENDS masodik-tests
        USES_TERMINAL
        COMMENT "A manual splithez kotodo natív regressziós tesztkör futtatása.")

    # T095 – cut23-tail tail split proof smoke target
    set(_masodik_split_proof_script "${PROJECT_SOURCE_DIR}/tools/scripts/validate-quickstart.ps1")
    add_custom_target(masodik-manual-split-proof-smoke
        COMMAND powershell.exe -ExecutionPolicy Bypass -File "${_masodik_split_proof_script}"
        DEPENDS masodik-tests
        USES_TERMINAL
        COMMENT "A cut23-tail split proof smoke validacioja (validate-quickstart.ps1).")

    set(_masodik_stream_smoke_script "${PROJECT_SOURCE_DIR}/tests/smoke/test_chat_completion_stream.ps1")
    set(_masodik_tool_call_smoke_script "${PROJECT_SOURCE_DIR}/tests/smoke/test_tool_calling.ps1")
    if(EXISTS "${_masodik_stream_smoke_script}")
        add_custom_target(masodik-streaming-smoke
            COMMAND powershell.exe -ExecutionPolicy Bypass -File "${_masodik_stream_smoke_script}"
                    -BaseUrl "http://127.0.0.1:29011"
                    -BodyPath "${PROJECT_SOURCE_DIR}/tests/fixtures/chat_request_stream.json"
            DEPENDS masodik-tests
            USES_TERMINAL
            COMMENT "A streaming smoke ellenorzo szkript futtatasa.")
    else()
        add_custom_target(
            masodik-streaming-smoke
            COMMAND "${CMAKE_COMMAND}" -E echo "A streaming smoke script meg nincs letrehozva: ${_masodik_stream_smoke_script}"
            DEPENDS masodik-tests
            USES_TERMINAL
            COMMENT "Streaming smoke belepesi pont elokeszitese.")
    endif()

    if(EXISTS "${_masodik_tool_call_smoke_script}")
        add_custom_target(masodik-tool-calling-smoke
            COMMAND powershell.exe -ExecutionPolicy Bypass -File "${_masodik_tool_call_smoke_script}"
                    -BaseUrl "http://127.0.0.1:29011"
                    -BodyPath "${PROJECT_SOURCE_DIR}/tests/fixtures/chat_request_tool_call.json"
            DEPENDS masodik-tests
            USES_TERMINAL
            COMMENT "A tool-calling smoke ellenorzo szkript futtatasa.")
    else()
        add_custom_target(
            masodik-tool-calling-smoke
            COMMAND "${CMAKE_COMMAND}" -E echo "A tool-calling smoke script meg nincs letrehozva: ${_masodik_tool_call_smoke_script}"
            DEPENDS masodik-tests
            USES_TERMINAL
            COMMENT "Tool-calling smoke belepesi pont elokeszitese.")
    endif()
else()
    add_custom_target(
        masodik-tests-bootstrap
        COMMENT "A tesztforrasok meg nem allnak rendelkezesre."
    )
endif()
