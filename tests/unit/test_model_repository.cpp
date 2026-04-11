#include "test_registry.hpp"

#include "masodik/config/default_paths.hpp"
#include "masodik/config/model_repository.hpp"

MASODIK_TEST(model_repository_detects_local_ai_playground_layout) {
    const auto repository = masodik::config::inspect_model_repository(masodik::config::default_paths::default_model_root);

    masodik::tests::require(repository.model_id == "gpt-oss-20b-int4-ov", "A modellazonosito nem vart.");
    masodik::tests::require(!repository.model_xml_path.empty(), "A model xml path nem lehet ures.");
    masodik::tests::require(!repository.model_bin_path.empty(), "A model bin path nem lehet ures.");
    masodik::tests::require(!repository.graph_path.empty(), "A graph path nem lehet ures.");
}
