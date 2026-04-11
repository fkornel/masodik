# Data Model: Helyi hibrid AI kiszolgalas

## 1. ToolchainProfile

**Cel**: A buildhez es fejlesztoi futtatashoz hasznalt natív toolchain rogzitese.

**Mezok**:
- `compiler_family`
- `compiler_version`
- `cl_path`
- `generator`
- `openvino_cmake_dir`
- `cmake_path`
- `ctest_path`
- `ninja_path`

## 2. ModelRepository

**Cel**: A mar letoltott, ujrafelhasznalt modellrepo leirasa.

**Mezok**:
- `model_id`
- `root_path`
- `model_xml_path`
- `model_bin_path`
- `tokenizer_json_path`
- `source`
- `read_only`

## 3. RuntimeDependencyReport

**Cel**: Az in-process GenAI fejleszteshez szukseges helyi fejlesztoi es runtime komponensek
elerhetosegenek rogzitese.

**Mezok**:
- `openvino_include_dir`
- `openvino_runtime_libs_dir`
- `openvino_genai_dll_path`
- `openvino_genai_c_dll_path`
- `openvino_genai_import_lib_path`
- `openvino_include_dir_exists`
- `openvino_runtime_libs_dir_exists`
- `openvino_genai_dll_exists`
- `openvino_genai_c_dll_exists`
- `openvino_genai_import_lib_exists`
- `model_repository_exists`
- `notes`

## 4. ServingProfile

**Cel**: A helyi kiszolgalasi policy es hardverhangolas leirasa.

**Mezok**:
- `profile_name`
- `model_name`
- `primary_device`
- `hybrid_enabled`
- `hybrid_fallback_device`
- `cache_directory`
- `runtime_log_path`
- `bind_host`
- `bind_port`
- `desired_context_tokens`
- `max_num_seqs`
- `prefix_caching_enabled`
- `queue_policy`
- `cpu_fallback_enabled`
- `timeout_seconds`
- `health_endpoint_path`
- `chat_endpoint_path`
- `models_endpoint_path`
- `notes`

## 5. GenerationConfig

**Cel**: Az egyes generacios futasokhoz hasznalt alap parameterek rogzitese.

**Mezok**:
- `primary_device`
- `hybrid_enabled`
- `hybrid_fallback_device`
- `cache_directory`
- `max_new_tokens`
- `desired_context_tokens`

## 6. ChatCompletionRequest

**Cel**: A kliens altal kuldott chat completion keres leirasa.

**Mezok**:
- `model`
- `messages`
- `max_tokens`
- `stream`
- `tools`
- `tool_choice`

## 7. ChatCompletionResponse

**Cel**: A helyi szolgaltatas altal visszaadott chat completion valasz.

**Mezok**:
- `id`
- `model`
- `choices`
- `tool_calls`
- `finish_reason`

## 8. ToolDefinition

**Cel**: A kliens altal elerhetokent meghirdetett egyedi tool schema rogzitese.

**Mezok**:
- `type`
- `function_name`
- `description`
- `parameters_schema`

## 9. ToolCall

**Cel**: A modell altal visszaadott strukturalt tool hivasi szandek leirasa.

**Mezok**:
- `id`
- `type`
- `function_name`
- `arguments_json`
- `index`

## 10. ToolCallStreamDelta

**Cel**: Egy streamelt tool call reszleges frissitesenek leirasa.

**Mezok**:
- `index`
- `id`
- `function_name_delta`
- `arguments_delta`
- `terminal`

## 11. RuntimeSession

**Cel**: Egy aktiv in-process szolgaltatasi futas allapotanak leirasa.

**Mezok**:
- `active_profile`
- `active_model`
- `runtime_state`
- `queue_depth`
- `ready`
- `last_error_category`
- `last_error_message`
- `gpu_offload_active`

## 12. RequestAuditRecord

**Cel**: Egy bejovo chat completion keres tool-calling szempontbol relevans diagnosztikai lenyomatanak rogzitese.

**Mezok**:
- `request_id`
- `stream`
- `tools_present`
- `tool_count`
- `tool_choice_kind` (`absent` | `none` | `auto` | `required` | `named_function`)
- `tool_choice_name`
- `decision_path` (`unknown` | `content_only` | `tool_call_auto` | `tool_call_required` | `tool_call_named`)
- `validation_outcome` (`not_evaluated` | `accepted` | `parser_error` | `tool_schema_error` | `model_selection_error` | `runtime_readiness_error` | `runtime_error`)
- `truncated_message_preview`
- `created_at`

## Kapcsolati osszefoglalas

- Egy `ToolchainProfile` es egy `RuntimeDependencyReport` egyutt hatarozza meg, hogy a helyi gep
  keszen all-e az in-process fejlesztesre.
- Egy `ModelRepository` egy vagy tobb `ServingProfile` alatt aktiv modellt kepviselhet.
- Egy `ServingProfile` es egy `GenerationConfig` hatarozza meg az aktualis generacios futas
  hardver- es timeout viselkedeset.
- Egy `ChatCompletionRequest` nulla vagy tobb `ToolDefinition` elemet hordozhat.
- Egy `ChatCompletionResponse` nulla vagy tobb `ToolCall` elemet tartalmazhat.
- Egy `ToolCall` streamelt uzemmodban egy vagy tobb `ToolCallStreamDelta` elembol allhat ossze.
- Egy `ChatCompletionRequest`-hez nulla vagy egy `RequestAuditRecord` tartozhat, amely a request
  szerzodes- es feldolgozasi allapotat diagnosztikai celra rogizti.
- Egy `ChatCompletionRequest` egy aktiv `RuntimeSession` alatt fut, es `ChatCompletionResponse`
  valasszal zarul.
