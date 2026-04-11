#include <stdexcept>

#include "test_registry.hpp"

#include "masodik/diagnostics/queue_metrics.hpp"
#include "masodik/runtime/request_scheduler.hpp"

MASODIK_TEST(manual_split_handoff_lease_releases_request_id_after_scope_exit) {
    masodik::diagnostics::QueueMetrics queue_metrics;
    masodik::runtime::QueuePolicy queue_policy;
    masodik::runtime::RequestScheduler scheduler{queue_policy, queue_metrics};

    {
        auto lease = scheduler.acquire_handoff(41, "seq-41");
        masodik::tests::require(lease.request_id() == 41, "A handoff lease request_id nem vart.");
        masodik::tests::require(lease.sequence_id() == "seq-41", "A handoff lease sequence_id nem vart.");
    }

    auto reacquired = scheduler.acquire_handoff(41, "seq-41b");
    masodik::tests::require(reacquired.sequence_id() == "seq-41b", "A handoff lease felszabaditas utan ujra foglalhatonak kell lennie.");
}

MASODIK_TEST(manual_split_handoff_rejects_duplicate_active_request_id) {
    masodik::diagnostics::QueueMetrics queue_metrics;
    masodik::runtime::QueuePolicy queue_policy;
    masodik::runtime::RequestScheduler scheduler{queue_policy, queue_metrics};

    auto lease = scheduler.acquire_handoff(99, "seq-99");
    bool threw = false;
    try {
        (void)scheduler.acquire_handoff(99, "seq-99-dupe");
    } catch (const std::runtime_error&) {
        threw = true;
    }

    masodik::tests::require(threw, "Az aktiv request_id-hez nem lehet masodik handoff lease-t nyitni.");
}

MASODIK_TEST(manual_split_queue_policy_reject_new_refuses_second_live_request) {
    masodik::diagnostics::QueueMetrics queue_metrics;
    masodik::runtime::QueuePolicy queue_policy;
    queue_policy.max_queued_requests = 1;
    queue_policy.max_concurrent_requests = 1;
    queue_policy.overflow_behavior = masodik::runtime::QueueOverflowBehavior::reject_new;
    queue_policy.max_queue_wait_seconds = 30;
    queue_policy.poll_interval_millis = 10;
    masodik::runtime::RequestScheduler scheduler{queue_policy, queue_metrics};

    auto first = scheduler.acquire(std::chrono::seconds{60});
    bool threw = false;
    try {
        (void)scheduler.acquire(std::chrono::seconds{60});
    } catch (const std::runtime_error& error) {
        threw = std::string{error.what()}.find("queue policy") != std::string::npos;
    }

    masodik::tests::require(threw, "A reject_new split queue policy-nak azonnal vissza kell utasitania a masodik aktiv kerest.");
    (void)first;
}
