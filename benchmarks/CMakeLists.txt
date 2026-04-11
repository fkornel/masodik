file(GLOB_RECURSE MASODIK_BENCHMARK_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if(MASODIK_BENCHMARK_SOURCES)
    add_executable(masodik-benchmarks ${MASODIK_BENCHMARK_SOURCES})
    target_include_directories(masodik-benchmarks PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/benchmarks")
    if(TARGET masodik::core)
        target_link_libraries(masodik-benchmarks PRIVATE masodik::core)
    else()
        target_link_libraries(masodik-benchmarks PRIVATE masodik::openvino)
    endif()
    masodik_deploy_runtime_dependencies(masodik-benchmarks)

    add_custom_target(masodik-streaming-benchmarks
        COMMAND "${CMAKE_COMMAND}" -E echo "Streaming benchmark entrypoint: benchmark_stream_first_token"
        COMMAND "${CMAKE_COMMAND}" -E echo "Streaming benchmark entrypoint: benchmark_streaming_mixed_load"
        COMMAND "${CMAKE_COMMAND}" -E echo "Tool-calling benchmark entrypoint: benchmark_tool_call_first_delta"
        DEPENDS masodik-benchmarks
        USES_TERMINAL
        COMMENT "A streaming benchmark belepesi pontok listazasa.")
    add_custom_target(masodik-manual-split-benchmarks
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_cold_start"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_long_context"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_handoff_latency"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_compiled_handoff"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_diagnostics_overhead"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_stability_proof"
        DEPENDS masodik-benchmarks
        USES_TERMINAL
        COMMENT "A manual split benchmark belepesi pontok listazasa.")
    add_custom_target(masodik-manual-split-regression
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split regression target: set MASODIK_MANUAL_SPLIT_MODEL_ROOT es MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT a valodi split artifact rootra."
        DEPENDS masodik-manual-split-benchmarks
        USES_TERMINAL
        COMMENT "A manual split regresszios benchmark entrypointok elokeszitese.")
else()
    add_custom_target(
        masodik-benchmarks-bootstrap
        COMMENT "A benchmark forrasfajlok meg nincsenek letrehozva."
    )
    add_custom_target(
        masodik-manual-split-benchmarks-bootstrap
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_cold_start"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_long_context"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_handoff_latency"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_compiled_handoff"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_diagnostics_overhead"
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split benchmark entrypoint: benchmark_manual_split_stability_proof"
        COMMENT "A manual split benchmark belepesi pontok elokeszitese.")
    add_custom_target(
        masodik-manual-split-regression-bootstrap
        COMMAND "${CMAKE_COMMAND}" -E echo "Manual split regression target: valodi split artifact root szukseges."
        COMMENT "A manual split regresszios target elokeszitese.")
endif()

