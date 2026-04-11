// T046 [US3] — Split health payload unit tesztek: a to_json(HealthResponse) kimenet
// tartalmazza az osszes split-specifikus mezot ervenyes es nem-split uzemmodban egyarant.

#include "test_registry.hpp"

#include "masodik/api/openai_contract.hpp"

MASODIK_TEST(split_health_payload_contains_all_split_fields_when_split_mode_active) {
    const masodik::api::HealthResponse response{
        .ready = true,
        .state = "ready",
        .active_model = "gpt-oss-20b-int4-ov",
        .active_profile = "b580-manual-split",
        .effective_device = "GPU.1+GPU.0",
        .queue_depth = 0,
        .active_streams = 0,
        .peak_active_streams = 1,
        .total_stream_failures = 0,
        .last_disconnect_error = 0,
        .gpu_offload_active = true,
        .status_message = "A pipeline keszen all.",
        .split_mode = true,
        .split_profile = "b580-manual-split",
        .stage_1_state = "ready",
        .stage_2_state = "ready",
        .handoff_state = "ready",
        .request_path_mode = "compiled",
        .stage_1_device = "GPU.1",
        .stage_2_device = "GPU.0",
        .stage_1_execution_devices = "GPU.1",
        .stage_2_execution_devices = "GPU.0",
        .split_artifact_version = "split-v1",
        .validated_context_tokens = 131072,
        .last_split_request_id = "splitreq-17",
        .last_split_sequence_id = "seq-17",
        .last_failure_scope = "",
        .fallback_detected = false,
        .memory_summary = {.stage_1 = "budget:12.0GB", .stage_2 = "budget:24.0GB", .system = "bounded"}};

    const auto json = masodik::api::to_json(response);

    masodik::tests::require(json.find("\"split_mode\":true") != std::string::npos,
                            "A split_mode mezonek true-nak kell lennie.");
    masodik::tests::require(json.find("\"split_profile\":\"b580-manual-split\"") != std::string::npos,
                            "A split_profile mezo nem vart.");
    masodik::tests::require(json.find("\"stage_1_state\":\"ready\"") != std::string::npos,
                            "A stage_1_state mezo nem vart.");
    masodik::tests::require(json.find("\"stage_2_state\":\"ready\"") != std::string::npos,
                            "A stage_2_state mezo nem vart.");
    masodik::tests::require(json.find("\"handoff_state\":\"ready\"") != std::string::npos,
                            "A handoff_state mezo nem vart.");
    masodik::tests::require(json.find("\"request_path_mode\":\"compiled\"") != std::string::npos,
                            "A request_path_mode mezo nem vart.");
    masodik::tests::require(json.find("\"stage_1_device\":\"GPU.1\"") != std::string::npos,
                            "A stage_1_device mezo nem vart.");
    masodik::tests::require(json.find("\"stage_2_device\":\"GPU.0\"") != std::string::npos,
                            "A stage_2_device mezo nem vart.");
    masodik::tests::require(json.find("\"stage_1_execution_devices\":\"GPU.1\"") != std::string::npos,
                            "A stage_1_execution_devices mezo nem vart.");
    masodik::tests::require(json.find("\"stage_2_execution_devices\":\"GPU.0\"") != std::string::npos,
                            "A stage_2_execution_devices mezo nem vart.");
    masodik::tests::require(json.find("\"split_artifact_version\":\"split-v1\"") != std::string::npos,
                            "A split_artifact_version mezo nem vart.");
    masodik::tests::require(json.find("\"validated_context_tokens\":131072") != std::string::npos,
                            "A validated_context_tokens mezo nem vart.");
    masodik::tests::require(json.find("\"last_split_request_id\":\"splitreq-17\"") != std::string::npos,
                            "A last_split_request_id mezo nem vart.");
    masodik::tests::require(json.find("\"last_split_sequence_id\":\"seq-17\"") != std::string::npos,
                            "A last_split_sequence_id mezo nem vart.");
    masodik::tests::require(json.find("\"last_failure_scope\":\"\"") != std::string::npos ||
                                json.find("\"last_failure_scope\":\"none\"") != std::string::npos,
                            "A last_failure_scope ures vagy none kell legyen sikeres startupnal.");
    masodik::tests::require(json.find("\"fallback_detected\":false") != std::string::npos,
                            "A fallback_detected false kell legyen.");
    masodik::tests::require(json.find("\"memory_summary\"") != std::string::npos,
                            "A memory_summary objektumnak leteznie kell.");
    masodik::tests::require(json.find("\"stage_1\":\"budget:12.0GB\"") != std::string::npos,
                            "A memory_summary.stage_1 nem vart.");
    masodik::tests::require(json.find("\"stage_2\":\"budget:24.0GB\"") != std::string::npos,
                            "A memory_summary.stage_2 nem vart.");
    masodik::tests::require(json.find("\"system\":\"bounded\"") != std::string::npos,
                            "A memory_summary.system nem vart.");
}

MASODIK_TEST(split_health_payload_omits_split_fields_when_split_mode_inactive) {
    const masodik::api::HealthResponse response{
        .ready = true,
        .state = "ready",
        .active_model = "gpt-oss-20b-int4-ov",
        .active_profile = "local-b580",
        .split_mode = false};

    const auto json = masodik::api::to_json(response);

    masodik::tests::require(json.find("\"split_mode\":false") != std::string::npos,
                            "A split_mode mezonek false-nak kell lennie.");
    masodik::tests::require(json.find("\"stage_1_state\":\"\"") != std::string::npos ||
                                json.find("\"stage_1_state\"") == std::string::npos ||
                                json.find("\"stage_1_state\":\"\"") != std::string::npos,
                            "A split-specifikus mezoknek uresnek vagy hianyzónak kell lenniuk nem-split modban.");
}

MASODIK_TEST(split_health_payload_reflects_stage_failure_state) {
    const masodik::api::HealthResponse response{
        .ready = false,
        .state = "failed",
        .active_model = "gpt-oss-20b-int4-ov",
        .active_profile = "b580-manual-split",
        .split_mode = true,
        .split_profile = "b580-manual-split",
        .stage_1_state = "ready",
        .stage_2_state = "failed",
        .handoff_state = "failed",
        .request_path_mode = "degraded",
        .last_failure_scope = "stage-2",
        .fallback_detected = false,
        .memory_summary = {.stage_1 = "budget:12.0GB", .stage_2 = "budget:24.0GB", .system = "bounded"}};

    const auto json = masodik::api::to_json(response);

    masodik::tests::require(json.find("\"ready\":false") != std::string::npos,
                            "A ready false kell legyen stage hibanalˇ.");
    masodik::tests::require(json.find("\"stage_1_state\":\"ready\"") != std::string::npos,
                            "Stage-1 ready kell legyen, ha csak stage-2 hibazott.");
    masodik::tests::require(json.find("\"stage_2_state\":\"failed\"") != std::string::npos,
                            "Stage-2 failed kell legyen.");
    masodik::tests::require(json.find("\"handoff_state\":\"failed\"") != std::string::npos,
                            "A handoff failed kell legyen, ha egy stage meghibasodott.");
    masodik::tests::require(json.find("\"request_path_mode\":\"degraded\"") != std::string::npos,
                            "A request_path_mode degraded kell legyen stage hiba eseten.");
    masodik::tests::require(json.find("\"last_failure_scope\":\"stage-2\"") != std::string::npos,
                            "A last_failure_scope stage-2 kell legyen.");
}

MASODIK_TEST(split_health_payload_reflects_fallback_detected) {
    const masodik::api::HealthResponse response{
        .ready = false,
        .state = "failed",
        .active_model = "gpt-oss-20b-int4-ov",
        .active_profile = "b580-manual-split",
        .split_mode = true,
        .split_profile = "b580-manual-split",
        .stage_1_state = "failed",
        .stage_2_state = "failed",
        .handoff_state = "failed",
        .last_failure_scope = "startup",
        .fallback_detected = true};

    const auto json = masodik::api::to_json(response);

    masodik::tests::require(json.find("\"fallback_detected\":true") != std::string::npos,
                            "A fallback_detected true kell legyen, ha CPU fallback-et eszlelt a startup.");
    masodik::tests::require(json.find("\"last_failure_scope\":\"startup\"") != std::string::npos,
                            "A startup scope kell legyen fallback eseten.");
}
