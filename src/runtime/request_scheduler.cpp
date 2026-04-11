#include "masodik/runtime/request_scheduler.hpp"

#include <algorithm>
#include <stdexcept>

namespace masodik::runtime {

RequestCancellationToken::RequestCancellationToken(const std::shared_ptr<std::atomic<bool>> cancelled) noexcept
    : cancelled_(cancelled) {}

RequestScheduler::HandoffLease::HandoffLease(
    RequestScheduler* scheduler,
    const std::uint64_t request_id,
    std::string sequence_id) noexcept
    : scheduler_(scheduler), request_id_(request_id), sequence_id_(std::move(sequence_id)) {}

RequestScheduler::HandoffLease::~HandoffLease() {
    reset();
}

RequestScheduler::HandoffLease::HandoffLease(HandoffLease&& other) noexcept
    : scheduler_(other.scheduler_), request_id_(other.request_id_), sequence_id_(std::move(other.sequence_id_)) {
    other.scheduler_ = nullptr;
    other.request_id_ = 0;
}

RequestScheduler::HandoffLease& RequestScheduler::HandoffLease::operator=(HandoffLease&& other) noexcept {
    if (this != &other) {
        reset();
        scheduler_ = other.scheduler_;
        request_id_ = other.request_id_;
        sequence_id_ = std::move(other.sequence_id_);
        other.scheduler_ = nullptr;
        other.request_id_ = 0;
    }
    return *this;
}

std::uint64_t RequestScheduler::HandoffLease::request_id() const noexcept { return request_id_; }

const std::string& RequestScheduler::HandoffLease::sequence_id() const noexcept { return sequence_id_; }

void RequestScheduler::HandoffLease::reset() noexcept {
    if (scheduler_ == nullptr) {
        return;
    }
    scheduler_->release_handoff(request_id_);
    scheduler_ = nullptr;
    request_id_ = 0;
}

void RequestCancellationToken::cancel() const noexcept {
    if (cancelled_) {
        cancelled_->store(true);
    }
}

bool RequestCancellationToken::cancelled() const noexcept { return cancelled_ && cancelled_->load(); }

RequestScheduler::Lease::Lease(
    RequestScheduler* scheduler,
    const bool waited,
    const int wait_millis,
    const std::uint64_t request_id,
    RequestCancellationToken cancellation_token) noexcept
    : scheduler_(scheduler),
      waited_(waited),
      wait_millis_(wait_millis),
      request_id_(request_id),
      cancellation_token_(std::move(cancellation_token)) {}

RequestScheduler::Lease::~Lease() {
    reset();
}

RequestScheduler::Lease::Lease(Lease&& other) noexcept
    : scheduler_(other.scheduler_),
      waited_(other.waited_),
      wait_millis_(other.wait_millis_),
      request_id_(other.request_id_),
      cancellation_token_(std::move(other.cancellation_token_)),
      streaming_(other.streaming_) {
    other.scheduler_ = nullptr;
    other.streaming_ = false;
}

RequestScheduler::Lease& RequestScheduler::Lease::operator=(Lease&& other) noexcept {
    if (this != &other) {
        reset();
        scheduler_ = other.scheduler_;
        waited_ = other.waited_;
        wait_millis_ = other.wait_millis_;
        request_id_ = other.request_id_;
        cancellation_token_ = std::move(other.cancellation_token_);
        streaming_ = other.streaming_;
        other.scheduler_ = nullptr;
        other.streaming_ = false;
    }
    return *this;
}

bool RequestScheduler::Lease::waited() const noexcept { return waited_; }

int RequestScheduler::Lease::wait_millis() const noexcept { return wait_millis_; }

std::uint64_t RequestScheduler::Lease::request_id() const noexcept { return request_id_; }

const RequestCancellationToken& RequestScheduler::Lease::cancellation_token() const noexcept { return cancellation_token_; }

bool RequestScheduler::Lease::streaming() const noexcept { return streaming_; }

void RequestScheduler::Lease::reset() noexcept {
    if (scheduler_ == nullptr) {
        return;
    }
    if (streaming_) {
        scheduler_->note_stream_finished();
        streaming_ = false;
    }
    scheduler_->release();
    scheduler_ = nullptr;
}

void RequestScheduler::Lease::begin_streaming() {
    if (scheduler_ == nullptr || streaming_) {
        return;
    }
    scheduler_->note_stream_started();
    streaming_ = true;
}

void RequestScheduler::Lease::end_streaming() noexcept {
    if (scheduler_ == nullptr || !streaming_) {
        return;
    }
    scheduler_->note_stream_finished();
    streaming_ = false;
}

RequestScheduler::RequestScheduler(
    QueuePolicy queue_policy,
    masodik::diagnostics::QueueMetrics& queue_metrics,
    const std::size_t execution_slots)
    : queue_policy_(std::move(queue_policy)),
      queue_metrics_(queue_metrics),
      execution_slots_(std::max<std::size_t>(1U, execution_slots)) {}

RequestScheduler::Lease RequestScheduler::acquire(const std::chrono::milliseconds timeout_budget, const RequestKind request_kind) {
    queue_policy_.validate();

    std::unique_lock lock{mutex_};
    const auto request_id = next_request_id_++;
    auto cancellation_state = std::make_shared<std::atomic<bool>>(false);
    if (running_requests_ < execution_slots_ && queued_requests_ == 0U) {
        ++running_requests_;
        queue_metrics_.set_running_requests(running_requests_);
        queue_metrics_.note_started_request();
        queue_metrics_.set_last_wait_millis(0);
        return Lease{this, false, 0, request_id, RequestCancellationToken{std::move(cancellation_state)}};
    }

    if (queued_requests_ >= queue_policy_.max_queued_requests) {
        throw std::runtime_error("A keres varakozasi sora megtelt.");
    }

    if (queue_policy_.overflow_behavior == QueueOverflowBehavior::reject_new) {
        throw std::runtime_error("A queue policy uj kerest nem enged a varakozasi sorba.");
    }

    if (timeout_budget < std::chrono::seconds{queue_policy_.max_queue_wait_seconds}) {
        throw std::runtime_error("A keres timeout budgetje rovidebb a megengedett queue varakozasnal.");
    }

    ++queued_requests_;
    waiting_requests_.push_back({.request_id = request_id, .request_kind = request_kind});
    queue_metrics_.set_queued_requests(queued_requests_);
    const auto wait_started_at = std::chrono::steady_clock::now();
    const auto wait_deadline = wait_started_at + std::chrono::seconds{queue_policy_.max_queue_wait_seconds};

    const auto acquired = slot_available_.wait_until(lock, wait_deadline, [this, request_id] {
        return can_grant_request(request_id);
    });

    const auto wait_millis = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wait_started_at).count());

    erase_waiting_request(request_id);
    queue_metrics_.set_last_wait_millis(wait_millis);

    if (!acquired) {
        throw std::runtime_error("A keres tul sokat vart a queue-ban.");
    }

    ++running_requests_;
    queue_metrics_.set_running_requests(running_requests_);
    queue_metrics_.note_started_request();
    return Lease{this, true, wait_millis, request_id, RequestCancellationToken{std::move(cancellation_state)}};
}

RequestScheduler::HandoffLease RequestScheduler::acquire_handoff(std::uint64_t request_id, std::string sequence_id) {
    std::scoped_lock lock{mutex_};
    if (request_id == 0U) {
        throw std::invalid_argument("A split handoff foglalashoz ervenyes request_id kotelezo.");
    }
    if (active_handoffs_.contains(request_id)) {
        throw std::runtime_error("Ehhez a request_id-hez mar tartozik aktiv split handoff.");
    }
    if (sequence_id.empty()) {
        sequence_id = "seq-" + std::to_string(request_id);
    }
    active_handoffs_.emplace(request_id, sequence_id);
    return HandoffLease{this, request_id, std::move(sequence_id)};
}

bool RequestScheduler::can_grant_request(const std::uint64_t request_id) const noexcept {
    if (running_requests_ >= execution_slots_ || waiting_requests_.empty()) {
        return false;
    }

    for (const auto& queued_request : waiting_requests_) {
        if (queued_request.request_kind == RequestKind::standard) {
            return queued_request.request_id == request_id;
        }
    }

    return waiting_requests_.front().request_id == request_id;
}

void RequestScheduler::erase_waiting_request(const std::uint64_t request_id) noexcept {
    const auto iterator = std::find_if(waiting_requests_.begin(), waiting_requests_.end(), [request_id](const QueuedRequest& queued_request) {
        return queued_request.request_id == request_id;
    });
    if (iterator == waiting_requests_.end()) {
        return;
    }
    waiting_requests_.erase(iterator);
    if (queued_requests_ > 0U) {
        --queued_requests_;
    }
    queue_metrics_.set_queued_requests(queued_requests_);
}

void RequestScheduler::release_handoff(const std::uint64_t request_id) noexcept {
    std::scoped_lock lock{mutex_};
    active_handoffs_.erase(request_id);
}

void RequestScheduler::release() noexcept {
    std::scoped_lock lock{mutex_};
    if (running_requests_ > 0U) {
        --running_requests_;
    }
    queue_metrics_.set_running_requests(running_requests_);
    queue_metrics_.note_completed_request();
    slot_available_.notify_one();
}

void RequestScheduler::note_stream_started() noexcept { queue_metrics_.note_stream_started(); }

void RequestScheduler::note_stream_finished() noexcept { queue_metrics_.note_stream_finished(); }

}  // namespace masodik::runtime
