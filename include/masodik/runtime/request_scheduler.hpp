#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "masodik/diagnostics/queue_metrics.hpp"
#include "masodik/runtime/queue_policy.hpp"

namespace masodik::runtime {

enum class RequestKind {
    standard,
    streaming
};

class RequestCancellationToken {
public:
    RequestCancellationToken() = default;
    explicit RequestCancellationToken(std::shared_ptr<std::atomic<bool>> cancelled) noexcept;

    void cancel() const noexcept;
    [[nodiscard]] bool cancelled() const noexcept;

private:
    std::shared_ptr<std::atomic<bool>> cancelled_;
};

class RequestScheduler {
public:
    class HandoffLease {
    public:
        HandoffLease() = default;
        HandoffLease(RequestScheduler* scheduler, std::uint64_t request_id, std::string sequence_id) noexcept;
        ~HandoffLease();

        HandoffLease(const HandoffLease&) = delete;
        HandoffLease& operator=(const HandoffLease&) = delete;
        HandoffLease(HandoffLease&& other) noexcept;
        HandoffLease& operator=(HandoffLease&& other) noexcept;

        [[nodiscard]] std::uint64_t request_id() const noexcept;
        [[nodiscard]] const std::string& sequence_id() const noexcept;

    private:
        void reset() noexcept;

        RequestScheduler* scheduler_ = nullptr;
        std::uint64_t request_id_ = 0;
        std::string sequence_id_;
    };

    class Lease {
    public:
        Lease() = default;
        Lease(
            RequestScheduler* scheduler,
            bool waited,
            int wait_millis,
            std::uint64_t request_id,
            RequestCancellationToken cancellation_token) noexcept;
        ~Lease();

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;

        [[nodiscard]] bool waited() const noexcept;
        [[nodiscard]] int wait_millis() const noexcept;
        [[nodiscard]] std::uint64_t request_id() const noexcept;
        [[nodiscard]] const RequestCancellationToken& cancellation_token() const noexcept;
        [[nodiscard]] bool streaming() const noexcept;
        void begin_streaming();
        void end_streaming() noexcept;

    private:
        void reset() noexcept;

        RequestScheduler* scheduler_ = nullptr;
        bool waited_ = false;
        int wait_millis_ = 0;
        std::uint64_t request_id_ = 0;
        RequestCancellationToken cancellation_token_{};
        bool streaming_ = false;
    };

    RequestScheduler(
        QueuePolicy queue_policy,
        masodik::diagnostics::QueueMetrics& queue_metrics,
        std::size_t execution_slots = 1U);

    [[nodiscard]] Lease acquire(std::chrono::milliseconds timeout_budget, RequestKind request_kind = RequestKind::standard);
    [[nodiscard]] HandoffLease acquire_handoff(std::uint64_t request_id, std::string sequence_id = {});

private:
    struct QueuedRequest {
        std::uint64_t request_id = 0;
        RequestKind request_kind = RequestKind::standard;
    };

    [[nodiscard]] bool can_grant_request(std::uint64_t request_id) const noexcept;
    void erase_waiting_request(std::uint64_t request_id) noexcept;
    void release_handoff(std::uint64_t request_id) noexcept;
    void release() noexcept;
    void note_stream_started() noexcept;
    void note_stream_finished() noexcept;

    QueuePolicy queue_policy_;
    masodik::diagnostics::QueueMetrics& queue_metrics_;
    std::size_t execution_slots_ = 1U;
    std::mutex mutex_;
    std::condition_variable slot_available_;
    std::vector<QueuedRequest> waiting_requests_;
    std::map<std::uint64_t, std::string> active_handoffs_;
    std::size_t queued_requests_ = 0;
    std::size_t running_requests_ = 0;
    std::uint64_t next_request_id_ = 1;
};

}  // namespace masodik::runtime
