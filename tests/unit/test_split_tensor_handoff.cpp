#include <stdexcept>

#include <openvino/openvino.hpp>

#include "test_registry.hpp"

#include "masodik/genai/generation_config.hpp"
#include "masodik/split/split_infer_chain.hpp"

MASODIK_TEST(split_tensor_handoff_contract_accepts_distinct_gpu_devices) {
    masodik::config::SplitArtifactManifest manifest;
    manifest.hidden_state_contract = "hs-v1";
    manifest.kv_contract = "kv-v1";
    manifest.artifact_version = "split-v1";
    manifest.validated_context_tokens = 131072;

    masodik::genai::GenerationConfig generation_config;
    generation_config.split_mode = true;
    generation_config.stage_1_device = "GPU.1";
    generation_config.stage_2_device = "GPU.0";

    const auto contract = masodik::split::build_split_handoff_contract(manifest, generation_config);
    masodik::split::validate_split_handoff_contract(contract);

    masodik::tests::require(contract.stage_1_device == "GPU.1", "A stage_1_device nem vart.");
    masodik::tests::require(contract.stage_2_device == "GPU.0", "A stage_2_device nem vart.");
    masodik::tests::require(contract.validated_context_tokens == 131072, "A validated_context_tokens nem vart.");
}

MASODIK_TEST(split_tensor_handoff_contract_rejects_same_device_or_cpu) {
    masodik::split::SplitHandoffContract contract{
        .hidden_state_contract = "hs-v1",
        .kv_contract = "kv-v1",
        .artifact_version = "split-v1",
        .stage_1_device = "GPU.1",
        .stage_2_device = "GPU.1",
        .validated_context_tokens = 131072};

    bool threw = false;
    try {
        masodik::split::validate_split_handoff_contract(contract);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    masodik::tests::require(threw, "Az azonos split stage device-ot el kell utasitani.");

    contract.stage_2_device = "CPU";
    threw = false;
    try {
        masodik::split::validate_split_handoff_contract(contract);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    masodik::tests::require(threw, "A CPU stage device-ot el kell utasitani.");
}

MASODIK_TEST(split_request_path_mode_reports_placeholder_and_compiled_states) {
    masodik::tests::require(
        masodik::split::to_string(masodik::split::infer_split_request_path_mode(true, false, true, false, false)) == "placeholder",
        "Placeholder stage mellett placeholder path-mode vart.");
    masodik::tests::require(
        masodik::split::to_string(masodik::split::infer_split_request_path_mode(true, true, false, false, false)) == "compiled",
        "Compiled split stage-ek mellett compiled path-mode vart.");
    masodik::tests::require(
        masodik::split::to_string(masodik::split::infer_split_request_path_mode(true, true, false, false, true)) == "degraded",
        "Runtime hiba mellett degraded path-mode vart.");
}

MASODIK_TEST(split_tensor_handoff_buffer_is_ram_backed_and_stable_after_source_mutation) {
    ov::Tensor source{ov::element::f32, ov::Shape{1, 2}};
    auto* source_data = source.data<float>();
    source_data[0] = 3.0f;
    source_data[1] = 7.0f;

    auto handoff = masodik::split::make_ram_backed_handoff_buffer(source, "hs-v1", "hidden_states_from_stage_1");
    source_data[0] = 99.0f;

    masodik::tests::require(handoff.byte_size == source.get_byte_size(), "A handoff byte_size nem vart.");
    masodik::tests::require(handoff.source_contract == "hs-v1", "A handoff source_contract nem vart.");
    masodik::tests::require(
        handoff.target_tensor_name == "hidden_states_from_stage_1",
        "A handoff target_tensor_name nem vart.");
    masodik::tests::require(
        masodik::split::describe_tensor_shape(handoff.tensor) == "[1x2]",
        "A handoff tensor shape leiras nem vart.");
    masodik::tests::require(
        handoff.tensor.data<float>()[0] == 3.0f,
        "A handoff tensornek RAM-backelt masolatnak kell maradnia.");
}
