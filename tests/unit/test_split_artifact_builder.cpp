#include "manual_split_test_support.hpp"

#include <openvino/openvino.hpp>

#include "masodik/split/split_artifact_builder.hpp"

MASODIK_TEST(split_artifact_builder_exports_two_stage_ir_with_filtered_layers) {
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("split-artifact-builder-source");
    const auto output_root = source_fixture.root_path / "generated-artifact";

    const auto result = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .validated_context_tokens = 32768,
    });

    masodik::tests::require(std::filesystem::exists(result.split_manifest_path), "A split manifestnek el kell keszulnie.");
    masodik::tests::require(
        result.manifest.cut_after_layer == "layers.17 -> layers.18",
        "A generated manifest cut_after_layer mezoje nem vart.");
    masodik::tests::require(
        result.manifest.stage_1.layer_range == "0-17",
        "A generated stage-1 layer_range mezoje nem vart.");
    masodik::tests::require(
        result.manifest.stage_2.layer_range == "18-18",
        "A generated stage-2 layer_range mezoje nem vart.");
    masodik::tests::require(
        result.manifest.source_model_checksum.rfind("fnv1a64:", 0) == 0,
        "A generated source_model_checksum prefixe nem vart.");

    ov::Core core;
    const auto stage_1_model = core.read_model(result.stage_1_root / "openvino_model.xml");
    const auto stage_2_model = core.read_model(result.stage_2_root / "openvino_model.xml");

    bool stage_1_has_layer_17 = false;
    bool stage_1_has_layer_18 = false;
    for (const auto& node : stage_1_model->get_ops()) {
        stage_1_has_layer_17 = stage_1_has_layer_17 ||
            node->get_friendly_name().find("__module.model.layers.17.") != std::string::npos;
        stage_1_has_layer_18 = stage_1_has_layer_18 ||
            node->get_friendly_name().find("__module.model.layers.18.") != std::string::npos;
    }
    masodik::tests::require(stage_1_has_layer_17, "A stage-1 IR-ben meg kell maradnia a 17. retegnek.");
    masodik::tests::require(!stage_1_has_layer_18, "A stage-1 IR nem tartalmazhatja a 18. reteget.");

    bool stage_2_has_layer_17 = false;
    bool stage_2_has_layer_18 = false;
    bool stage_2_has_handoff_input = false;
    for (const auto& node : stage_2_model->get_ops()) {
        stage_2_has_layer_17 = stage_2_has_layer_17 ||
            node->get_friendly_name().find("__module.model.layers.17.") != std::string::npos;
        stage_2_has_layer_18 = stage_2_has_layer_18 ||
            node->get_friendly_name().find("__module.model.layers.18.") != std::string::npos;
        stage_2_has_handoff_input = stage_2_has_handoff_input ||
            node->get_friendly_name() == "hidden_states_from_stage_1";
    }
    masodik::tests::require(!stage_2_has_layer_17, "A stage-2 IR nem tartalmazhatja a 17. reteget.");
    masodik::tests::require(stage_2_has_layer_18, "A stage-2 IR-ben meg kell maradnia a 18. retegnek.");
    masodik::tests::require(stage_2_has_handoff_input, "A stage-2 IR-ben handoff Parameter vart.");
}

MASODIK_TEST(split_artifact_builder_supports_tail_split_after_last_layer) {
    const auto source_fixture = masodik::tests::create_manual_split_source_fixture("split-artifact-builder-tail-source");
    const auto output_root = source_fixture.root_path / "generated-tail-artifact";

    const auto result = masodik::split::build_split_artifact({
        .source_model_root = source_fixture.root_path,
        .output_root = output_root,
        .cut_after_layer = "layers.18 -> tail",
        .validated_context_tokens = 32768,
    });

    masodik::tests::require(
        result.manifest.cut_after_layer == "layers.18 -> tail",
        "A tail split manifest cut_after_layer mezoje nem vart.");
    masodik::tests::require(
        result.manifest.stage_1.layer_range == "0-18",
        "A tail split stage-1 layer_range mezoje nem vart.");
    masodik::tests::require(
        result.manifest.stage_2.layer_range == "tail",
        "A tail split stage-2 layer_range mezoje nem vart.");

    ov::Core core;
    const auto stage_2_model = core.read_model(result.stage_2_root / "openvino_model.xml");

    bool stage_2_has_layer_18 = false;
    bool stage_2_has_handoff_input = false;
    for (const auto& node : stage_2_model->get_ops()) {
        stage_2_has_layer_18 = stage_2_has_layer_18 ||
            node->get_friendly_name().find("__module.model.layers.18.") != std::string::npos;
        stage_2_has_handoff_input = stage_2_has_handoff_input ||
            node->get_friendly_name() == "hidden_states_from_stage_1";
    }

    masodik::tests::require(!stage_2_has_layer_18, "A tail stage-2 IR nem tartalmazhat transformer reteget.");
    masodik::tests::require(stage_2_has_handoff_input, "A tail stage-2 IR-ben handoff Parameter vart.");
}
