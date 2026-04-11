#include "test_registry.hpp"

#include "masodik/config/default_paths.hpp"
#include "masodik/config/model_repository.hpp"
#include "masodik/config/runtime_dependency_report.hpp"
#include "masodik/config/toolchain_profile.hpp"

MASODIK_TEST(runtime_dependency_report_detects_local_genai_bundle) {
    const auto toolchain = masodik::config::discover_default_toolchain_profile();
    const auto repository = masodik::config::inspect_model_repository(masodik::config::default_paths::default_model_root);
    const auto report = masodik::config::inspect_runtime_dependency_report(toolchain, repository);

    masodik::tests::require(report.openvino_include_dir_exists, "Az OpenVINO include konyvtar nem latszik a reportban.");
    masodik::tests::require(report.openvino_runtime_libs_dir_exists, "Az OpenVINO runtime DLL konyvtar nem latszik.");
    masodik::tests::require(report.openvino_genai_dll_exists, "Az openvino_genai.dll nem talalhato.");
    masodik::tests::require(report.openvino_genai_c_dll_exists, "Az openvino_genai_c.dll nem talalhato.");
    masodik::tests::require(report.model_repository_exists, "A lokal modellrepo nem talalhato.");
    masodik::tests::require(report.ready_for_in_process_runtime(), "A runtime report szerint a helyi in-process futas nem kesz.");
}

MASODIK_TEST(runtime_dependency_report_surfaces_missing_model_and_cmake_inputs) {
    auto toolchain = masodik::config::discover_default_toolchain_profile();
    toolchain.openvino_cmake_dir = masodik::tests::find_repo_root() / "build" / "missing-openvino-cmake";

    masodik::config::ModelRepository repository;
    repository.model_id = "missing-model";
    repository.root_path = masodik::tests::find_repo_root() / "build" / "missing-model";

    const auto report = masodik::config::inspect_runtime_dependency_report(toolchain, repository);

    masodik::tests::require(!report.model_repository_exists, "A hianyzo modellrepo nem lehetne ervenyes.");
    masodik::tests::require(!report.ready_for_in_process_runtime(), "A hianyos report nem jelezhet kesz runtime-ot.");
    masodik::tests::require(!report.notes.empty(), "A hianyzo dependency-krol vartunk magyar nyelvu megjegyzeseket.");
}
