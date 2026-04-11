#include <chrono>
#include <string_view>

#include "in_process_server_fixture.hpp"
#include "streaming_test_support.hpp"
#include "test_registry.hpp"

#include "masodik/config/json_loader.hpp"
#include "masodik/http/http_client.hpp"

namespace {

const masodik::config::JsonValue::Object& first_choice_object(const masodik::config::JsonValue& root) {
    const auto& choices = masodik::config::require_member(root.as_object(), "choices");
    masodik::tests::require(choices.is_array(), "A valasz choices mezoje nem tomb.");
    masodik::tests::require(!choices.as_array().empty(), "A valasz choices tombje ures.");
    masodik::tests::require(choices.as_array().front().is_object(), "Az elso choice nem objektum.");
    return choices.as_array().front().as_object();
}

}  // namespace

MASODIK_TEST(tool_calling_route_returns_non_stream_tool_call_without_execution) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-route-01", .bind_port = 29125});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29125, fixture.root_path};
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Sorold fel a nyitott PR-eket a megfelelo toollal."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto","stream":false})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "A non-stream tool-calling keres nem adott 200-at.");
    const auto root = masodik::config::JsonLoader::parse(response.body);
    const auto& choice = first_choice_object(root);
    const auto& message = masodik::config::require_member(choice, "message");
    masodik::tests::require(message.is_object(), "A tool-calling valasz message mezoje nem objektum.");
    const auto& message_object = message.as_object();
    masodik::tests::require(
        masodik::config::require_member(message_object, "content").is_null(),
        "A non-stream tool-calling valaszban a content mezot nullkent kell visszaadni.");

    const auto& tool_calls = masodik::config::require_member(message_object, "tool_calls");
    masodik::tests::require(tool_calls.is_array(), "A non-stream tool-calling valaszbol hianyzik a tool_calls tomb.");
    masodik::tests::require(tool_calls.as_array().size() == 1U, "Pontosan egy tool_call vart.");
    const auto& tool_call = tool_calls.as_array().front().as_object();
    const auto& function = masodik::config::require_member(tool_call, "function").as_object();

    masodik::tests::require(
        masodik::config::get_string(function, "name") == "exec",
        "A non-stream tool-calling valasz nem az elvart toolt valasztotta.");
    masodik::tests::require(
        masodik::config::get_string(function, "arguments").find("gh pr list") != std::string::npos,
        "A non-stream tool-calling arguments nem tartalmazza a vart parancsot.");
    masodik::tests::require(
        masodik::config::get_string(choice, "finish_reason") == "tool_calls",
        "A non-stream tool-calling finish_reason nem tool_calls.");
    masodik::tests::require(
        response.body.find("\"number\":") == std::string::npos,
        "A szerver nem hajthatta vegre a toolt, de eredmeny-szeru tartalom jelent meg.");
}

MASODIK_TEST(tool_calling_route_rejects_invalid_named_tool_choice_payload) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-route-02", .bind_port = 29126});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29126, fixture.root_path};
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Kerj tool hivast."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}}}}}],"tool_choice":{"type":"function","function":{"name":"missing"}}})",
        std::chrono::seconds{30});

    masodik::tests::require(response.status_code == 400, "A hibas named tool_choice payloadnak 400-as valaszt kell adnia.");
    masodik::tests::require(
        response.body.find("\"code\":\"tool_validation_error\"") != std::string::npos,
        "A hibas named tool_choice payload nem tool_validation_error kodot adott.");
}

MASODIK_TEST(tool_calling_route_prefers_file_tool_for_agent_md_requests) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-route-03", .bind_port = 29129});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29129, fixture.root_path};
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"check your AGENT.md"}],"tools":[{"type":"function","function":{"name":"web_search","description":"Webes kereses","parameters":{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}}},{"type":"function","function":{"name":"read_file","description":"Olvass helyi fajlt path alapjan","parameters":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}}}],"tool_choice":"auto","stream":false})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "Az AGENT.md tool-calling keres nem adott 200-at.");
    const auto root = masodik::config::JsonLoader::parse(response.body);
    const auto& choice = first_choice_object(root);
    const auto& message_object = masodik::config::require_member(choice, "message").as_object();
    const auto& tool_call = masodik::config::require_member(message_object, "tool_calls").as_array().front().as_object();
    const auto& function = masodik::config::require_member(tool_call, "function").as_object();

    masodik::tests::require(
        masodik::config::get_string(function, "name") == "read_file",
        "Az AGENT.md kereshez a file-orientalt toolt kellett volna valasztani.");
    masodik::tests::require(
        masodik::config::get_string(function, "arguments").find("AGENT.md") != std::string::npos,
        "Az AGENT.md keres arguments payloadja nem a vart fajlutra mutat.");
}

MASODIK_TEST(tool_calling_route_prefers_shell_tool_for_ssh_requests) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-route-04", .bind_port = 29130});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29130, fixture.root_path};
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"open ssh for 192.168.1.10"}],"tools":[{"type":"function","function":{"name":"web_search","description":"Webes kereses","parameters":{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}}},{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto","stream":false})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "Az SSH tool-calling keres nem adott 200-at.");
    const auto root = masodik::config::JsonLoader::parse(response.body);
    const auto& choice = first_choice_object(root);
    const auto& message_object = masodik::config::require_member(choice, "message").as_object();
    const auto& tool_call = masodik::config::require_member(message_object, "tool_calls").as_array().front().as_object();
    const auto& function = masodik::config::require_member(tool_call, "function").as_object();

    masodik::tests::require(
        masodik::config::get_string(function, "name") == "exec",
        "Az SSH kereshez a shell-orientalt toolt kellett volna valasztani.");
    masodik::tests::require(
        masodik::config::get_string(function, "arguments").find("ssh 192.168.1.10") != std::string::npos,
        "Az SSH keres arguments payloadja nem a vart hosztot tartalmazza.");
}

MASODIK_TEST(tool_calling_route_prefers_web_search_for_web_queries) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-route-05", .bind_port = 29131});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29131, fixture.root_path};
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"search on the web for Stable Diffusion 3.5 release date"}],"tools":[{"type":"function","function":{"name":"github_search","description":"GitHub issue es repo kereses","parameters":{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}}},{"type":"function","function":{"name":"web_search","description":"Webes kereses az interneten","parameters":{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}}},{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto","stream":false})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "A web-search tool-calling keres nem adott 200-at.");
    const auto root = masodik::config::JsonLoader::parse(response.body);
    const auto& choice = first_choice_object(root);
    const auto& message_object = masodik::config::require_member(choice, "message").as_object();
    const auto& tool_call = masodik::config::require_member(message_object, "tool_calls").as_array().front().as_object();
    const auto& function = masodik::config::require_member(tool_call, "function").as_object();

    masodik::tests::require(
        masodik::config::get_string(function, "name") == "web_search",
        "A webes kereseshez a web_search toolt kellett volna valasztani.");
    masodik::tests::require(
        masodik::config::get_string(function, "arguments").find("Stable Diffusion 3.5") != std::string::npos,
        "A web_search arguments payloadja nem tartalmazza az eredeti keresest.");
}

MASODIK_TEST(tool_calling_route_does_not_repeat_auto_tool_call_after_tool_turn) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-route-06", .bind_port = 29132});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29132, fixture.root_path};
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"search on the web for Stable Diffusion 3.5 release date"},{"role":"assistant","content":null,"tool_calls":[{"id":"call_web_search_0","type":"function","function":{"name":"web_search","arguments":"{\"query\":\"search on the web for Stable Diffusion 3.5 release date\"}"}}]},{"role":"tool","content":"{\"results\":[{\"title\":\"Release\",\"url\":\"https://example.invalid\"}]}"}],"tools":[{"type":"function","function":{"name":"web_search","description":"Webes kereses az interneten","parameters":{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}}},{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}}],"tool_choice":"auto","stream":false})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "A tool eredmeny utani chat keres nem adott 200-at.");
    masodik::tests::require(
        response.body.find("\"tool_calls\"") == std::string::npos,
        "A szerver nem bocsathat ki uj auto tool_call-t ugyanahhoz a mar tool eredmennyel rendelkezo turnhoz.");
}

MASODIK_TEST(tool_calling_route_prefers_exact_path_file_tool_for_explicit_read_requests) {
    masodik::tests::ScopedEnvVar disable_genai{"MASODIK_DISABLE_OPENVINO_GENAI", "1"};
    const auto fixture = masodik::tests::create_manual_split_fixture({.name = "tool-route-07", .bind_port = 29133});
    masodik::tests::InProcessServerFixture host{fixture.profile_path, 29133, fixture.root_path};
    masodik::http::HttpClient client;

    const auto response = client.post_json(
        host.service_config().chat_url,
        R"({"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Read HEARTBEAT.md if it exists (workspace context). Follow it strictly. When reading HEARTBEAT.md, use workspace file /home/node/.openclaw/workspace-light/HEARTBEAT.md (exact case). Do not read docs/heartbeat.md."}],"tools":[{"type":"function","function":{"name":"exec","description":"Lokalis shell parancs","parameters":{"type":"object","properties":{"command":{"type":"string"}},"required":["command"]}}},{"type":"function","function":{"name":"view","description":"View file or directory content by path","parameters":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}}}],"tool_choice":"auto","stream":false})",
        std::chrono::minutes{10});

    masodik::tests::require(response.status_code == 200, "Az explicit workspace-path read keres nem adott 200-at.");
    const auto root = masodik::config::JsonLoader::parse(response.body);
    const auto& choice = first_choice_object(root);
    const auto& message_object = masodik::config::require_member(choice, "message").as_object();
    const auto& tool_call = masodik::config::require_member(message_object, "tool_calls").as_array().front().as_object();
    const auto& function = masodik::config::require_member(tool_call, "function").as_object();

    masodik::tests::require(
        masodik::config::get_string(function, "name") == "view",
        "Az explicit file path olvasasahoz a path-alapu view toolt kellett volna valasztani.");
    masodik::tests::require(
        masodik::config::get_string(function, "arguments").find("/home/node/.openclaw/workspace-light/HEARTBEAT.md") !=
            std::string::npos,
        "A view tool arguments payloadja nem tartalmazza az explicit HEARTBEAT.md eleresi utat.");
}
