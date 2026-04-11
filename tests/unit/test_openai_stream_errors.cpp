#include <stdexcept>

#include "test_registry.hpp"

#include "masodik/server/openai_error_mapper.hpp"

MASODIK_TEST(openai_stream_errors_map_pre_stream_json_and_mid_stream_sse_payloads) {
    const std::invalid_argument bad_request{"A modellazonosito ervenytelen."};
    const std::logic_error not_ready{"A szolgaltatas nem ready."};
    const std::runtime_error mid_stream_failure{"A streaming generalas kozben hiba tortent."};

    const auto bad_request_response = masodik::server::map_exception_to_response(bad_request);
    const auto not_ready_response = masodik::server::map_exception_to_response(not_ready);
    const auto terminal_event = masodik::server::map_exception_to_terminal_sse_event(mid_stream_failure);

    masodik::tests::require(bad_request_response.status_code == 400, "A pre-stream validacios hiba nem 400-as.");
    masodik::tests::require(
        bad_request_response.body.find("\"code\":\"bad_request\"") != std::string::npos,
        "A pre-stream validacios hiba nem bad_request kodot adott.");
    masodik::tests::require(not_ready_response.status_code == 503, "A not-ready pre-stream hiba nem 503-as.");
    masodik::tests::require(
        not_ready_response.body.find("\"code\":\"service_unavailable\"") != std::string::npos,
        "A not-ready pre-stream hiba nem service_unavailable kodot adott.");
    masodik::tests::require(
        terminal_event.find("\"code\":\"runtime_error\"") != std::string::npos,
        "A mid-stream terminalis hiba nem runtime_error kodot adott.");
    masodik::tests::require(
        terminal_event.find("\"terminal\":true") != std::string::npos,
        "A mid-stream SSE hiba nem terminalis payload.");
    masodik::tests::require(
        terminal_event.rfind("\n\n") == terminal_event.size() - 2,
        "A mid-stream SSE hibaesemeny nem szabalyos SSE lezaras.");
}

MASODIK_TEST(openai_stream_errors_use_tool_specific_codes_when_context_is_known) {
    const std::invalid_argument invalid_tool{"A tools[].function.name ervenytelen."};
    const std::runtime_error streaming_tool_failure{"A tool-call delta serializacio megszakadt."};

    const auto validation_response =
        masodik::server::map_exception_to_response(invalid_tool, {.tool_call_related = true});
    const auto streaming_event = masodik::server::map_exception_to_terminal_sse_event(
        streaming_tool_failure,
        {.tool_call_related = true, .streaming = true});

    masodik::tests::require(
        validation_response.body.find("\"code\":\"tool_validation_error\"") != std::string::npos,
        "A tool validacios hiba nem tool_validation_error kodot kapott.");
    masodik::tests::require(
        streaming_event.find("\"code\":\"tool_stream_error\"") != std::string::npos,
        "A streaming tool hiba nem tool_stream_error kodot kapott.");
}

MASODIK_TEST(openai_stream_errors_map_split_specific_budget_handoff_and_overload_codes) {
    const std::runtime_error memory_budget_error{"A split stage-1 memory budget tullepve: required=14GB budget=12GB"};
    const std::runtime_error handoff_error{"A split handoff foglalas sikertelen: duplicate active request."};
    const std::runtime_error overload_error{"A queue policy uj kerest nem enged a varakozasi sorba."};

    const auto memory_budget_response = masodik::server::map_exception_to_response(memory_budget_error);
    const auto handoff_response = masodik::server::map_exception_to_response(handoff_error);
    const auto overload_response = masodik::server::map_exception_to_response(overload_error);

    masodik::tests::require(
        memory_budget_response.body.find("\"code\":\"split_memory_budget_error\"") != std::string::npos,
        "A split memoria-budget hiba nem kapott kulon hibakodot.");
    masodik::tests::require(
        handoff_response.body.find("\"code\":\"split_handoff_error\"") != std::string::npos,
        "A split handoff hiba nem kapott kulon hibakodot.");
    masodik::tests::require(
        overload_response.body.find("\"code\":\"split_overload_error\"") != std::string::npos,
        "A split overload hiba nem kapott kulon hibakodot.");
}
