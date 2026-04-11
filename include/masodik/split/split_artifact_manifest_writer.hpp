#pragma once

#include <filesystem>
#include <string>

#include "masodik/config/model_repository.hpp"

namespace masodik::split {

[[nodiscard]] std::string render_split_artifact_manifest_json(const masodik::config::SplitArtifactManifest& manifest);

void write_split_artifact_manifest(
    const masodik::config::SplitArtifactManifest& manifest,
    const std::filesystem::path& split_root,
    const std::filesystem::path& manifest_path = {});

}  // namespace masodik::split
