#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace masodik::genai {

/// Az embedding pipeline aktualis allapota.
enum class EmbeddingPipelineState {
    uninitialized,
    loading,
    ready,
    degraded,
    failed
};

/// Embedding eredmeny egyetlen szoveghez.
struct EmbeddingResult {
    std::vector<float> embedding;
    int token_count = 0;
};

/// Batch embedding eredmeny.
struct EmbeddingBatchResult {
    std::vector<EmbeddingResult> results;
    int total_tokens = 0;
};

/// Modell informaciok az embedding pipeline-bol.
struct EmbeddingModelInfo {
    std::string model_name;
    std::string device;
    int embedding_dimension = 0;
};

/// Az NPU embedding pipeline service, amely az ov::genai::TextEmbeddingPipeline-t wrapeli.
/// Fuggetlen a chat LlmPipelineService-tol — az embedding hiba nem hat a chat-re.
class EmbeddingPipelineService {
public:
    EmbeddingPipelineService();
    ~EmbeddingPipelineService();

    /// NPU modell betoltese. Ha a betoltes sikertelen, az allapot "failed" lesz.
    void initialize(
        const std::filesystem::path& model_path,
        const std::string& device,
        const std::string& model_name,
        bool reject_cpu_fallback);

    /// Batch embedding: tobb szoveg → tobb vektor + token szamok.
    [[nodiscard]] EmbeddingBatchResult embed(const std::vector<std::string>& texts) const;

    /// Pipeline keszsegi allapot.
    [[nodiscard]] bool ready() const noexcept;

    /// Allapot string ("ready", "failed", "uninitialized", stb.)
    [[nodiscard]] std::string_view state_string() const noexcept;

    /// Allapot enum.
    [[nodiscard]] EmbeddingPipelineState state() const noexcept;

    /// Modell informaciok (nev, eszkoz, dimenzio).
    [[nodiscard]] EmbeddingModelInfo model_info() const noexcept;

    /// Utolso hiba uzenet (ha volt).
    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    EmbeddingPipelineState state_ = EmbeddingPipelineState::uninitialized;
    std::string model_name_;
    std::string device_;
    int embedding_dimension_ = 0;
    std::string last_error_;
};

}  // namespace masodik::genai
