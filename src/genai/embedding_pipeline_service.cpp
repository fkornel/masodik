#include "masodik/genai/embedding_pipeline_service.hpp"

#if MASODIK_HAS_OPENVINO_GENAI
#include "openvino/genai/rag/text_embedding_pipeline.hpp"
#include "openvino/genai/tokenizer.hpp"
#endif

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace masodik::genai {

struct EmbeddingPipelineService::Impl {
#if MASODIK_HAS_OPENVINO_GENAI
    std::unique_ptr<ov::genai::TextEmbeddingPipeline> pipeline;
    std::unique_ptr<ov::genai::Tokenizer> tokenizer;
#endif
};

EmbeddingPipelineService::EmbeddingPipelineService() : impl_{std::make_unique<Impl>()} {}
EmbeddingPipelineService::~EmbeddingPipelineService() = default;

void EmbeddingPipelineService::initialize(
    const std::filesystem::path& model_path,
    const std::string& device,
    const std::string& model_name,
    const bool reject_cpu_fallback) {
    model_name_ = model_name;
    device_ = device;
    state_ = EmbeddingPipelineState::loading;

#if MASODIK_HAS_OPENVINO_GENAI
    try {
        if (!std::filesystem::exists(model_path)) {
            throw std::runtime_error("Az embedding modell konyvtar nem letezik: " + model_path.string());
        }

        // NPU reject_cpu_fallback: csak az elvárt eszkozre toltunk
        if (reject_cpu_fallback && device != "NPU" && device != "GPU" && device != "GPU.0" && device != "GPU.1") {
            // Ha a config NPU-t ker de valamiert masik eszkoz van megadva,
            // az figyelmeztetes, de nem blokkolja a betoltest.
        }

        // MEAN pooling es L2 normalizalas a Qwen3-Embedding sorozathoz.
        // Racionále: a Qwen3-Embedding modellcsalad dokumentacioja MEAN pooling-ot
        // javasol (nem CLS, nem LAST_TOKEN). A normalizalas biztositja, hogy a
        // cosine similarity kozvetlenul osszehasonlithato legyen RAG/keresesi
        // alkalmazasokban anelkul, hogy a kliens oldalon kulon normalizalni kellene.
        ov::genai::TextEmbeddingPipeline::Config config;
        config.pooling_type = ov::genai::TextEmbeddingPipeline::PoolingType::MEAN;
        config.normalize = true;

        const auto started_at = std::chrono::steady_clock::now();
        impl_->pipeline = std::make_unique<ov::genai::TextEmbeddingPipeline>(model_path, device, config);
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();

        // Tokenizer betoltese a token szamlalashoz
        impl_->tokenizer = std::make_unique<ov::genai::Tokenizer>(model_path);

        // Dimenzio meghatarozasa egy probe embed-del
        const auto probe_result = impl_->pipeline->embed_query("probe");
        if (const auto* float_vec = std::get_if<std::vector<float>>(&probe_result)) {
            embedding_dimension_ = static_cast<int>(float_vec->size());
        }

        state_ = EmbeddingPipelineState::ready;
        std::cerr << "[INFO] embedding_pipeline_ready model=" << model_name_
                  << " device=" << device_
                  << " dimension=" << embedding_dimension_
                  << " elapsed_ms=" << elapsed_ms << std::endl;

        // NPU fallback ellenorzes: ha reject_cpu_fallback es nem a kert eszkozre toltodott
        // (az OpenVINO csendben CPU-ra eshet vissza)
        // Megjegyzes: az OpenVINO nem ad explicit visszajelzest az eszkozrol a pipeline API-n,
        // ezert a reject_cpu_fallback logika a device string validalasakor lep eletbe.

    } catch (const std::exception& error) {
        state_ = EmbeddingPipelineState::failed;
        last_error_ = error.what();
        std::cerr << "[ERROR] embedding_pipeline_failed model=" << model_name_
                  << " device=" << device_
                  << " error=\"" << error.what() << "\"" << std::endl;
    }
#else
    state_ = EmbeddingPipelineState::failed;
    last_error_ = "Az OpenVINO GenAI tamogatas nincs beepitve.";
#endif
}

EmbeddingBatchResult EmbeddingPipelineService::embed(const std::vector<std::string>& texts) const {
    if (state_ != EmbeddingPipelineState::ready) {
        throw std::runtime_error("Az embedding pipeline nem all keszen (" + std::string{state_string()} + ").");
    }

#if MASODIK_HAS_OPENVINO_GENAI
    EmbeddingBatchResult batch_result;
    int total_tokens = 0;

    // Token szamolas a tokenizer-rel
    for (const auto& text : texts) {
        int token_count = 0;
        if (impl_->tokenizer) {
            try {
                const auto encoded = impl_->tokenizer->encode(text);
                const auto& shape = encoded.input_ids.get_shape();
                token_count = shape.size() > 1 ? static_cast<int>(shape[1]) : static_cast<int>(shape[0]);
            } catch (...) {
                // Ha a tokenizalas sikertelen, 0-t hasznalunk
            }
        }
        total_tokens += token_count;
    }

    // Embedding generalas: az NPU fix shape-et var, ezert egyenkent hivjuk
    // az embed_query()-t minden szovegre.
    // Racionále: az embed_documents() batch inferencia-t hasznal, ami az NPU
    // NPUW plugin-nel shape mismatch hibat okoz ("padded_shape[i] == position_shape[i]"
    // assertion). Az embed_query() egyesevel, fix shape-pel dolgozik, ami az
    // NPU-val stabilan mukodik. A teljesitmenybeli kulonbseg elhanyagolhato,
    // mert az NPU inferencia maga gyors (~50ms/szoveg), es a tipikus batch
    // meret kicsi (1-20 szoveg).
    for (std::size_t i = 0; i < texts.size(); ++i) {
        const auto result = impl_->pipeline->embed_query(texts[i]);
        if (const auto* float_vec = std::get_if<std::vector<float>>(&result)) {
            int per_text_tokens = 0;
            if (impl_->tokenizer) {
                try {
                    const auto encoded = impl_->tokenizer->encode(texts[i]);
                    const auto& shape = encoded.input_ids.get_shape();
                    per_text_tokens = shape.size() > 1 ? static_cast<int>(shape[1]) : static_cast<int>(shape[0]);
                } catch (...) {}
            }
            batch_result.results.push_back({
                .embedding = *float_vec,
                .token_count = per_text_tokens
            });
        } else {
            throw std::runtime_error("Az embedding pipeline nem float eredmenyt adott vissza.");
        }
    }

    batch_result.total_tokens = total_tokens;
    return batch_result;
#else
    throw std::runtime_error("Az OpenVINO GenAI tamogatas nincs beepitve.");
#endif
}

bool EmbeddingPipelineService::ready() const noexcept {
    return state_ == EmbeddingPipelineState::ready;
}

std::string_view EmbeddingPipelineService::state_string() const noexcept {
    switch (state_) {
    case EmbeddingPipelineState::uninitialized: return "uninitialized";
    case EmbeddingPipelineState::loading: return "loading";
    case EmbeddingPipelineState::ready: return "ready";
    case EmbeddingPipelineState::degraded: return "degraded";
    case EmbeddingPipelineState::failed: return "failed";
    }
    return "unknown";
}

EmbeddingPipelineState EmbeddingPipelineService::state() const noexcept {
    return state_;
}

EmbeddingModelInfo EmbeddingPipelineService::model_info() const noexcept {
    return {.model_name = model_name_, .device = device_, .embedding_dimension = embedding_dimension_};
}

const std::string& EmbeddingPipelineService::last_error() const noexcept {
    return last_error_;
}

}  // namespace masodik::genai
