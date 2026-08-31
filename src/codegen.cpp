#include "codegen.h"

#include <cctype>
#include <sstream>

namespace rocket {

std::string BootstrapCodeGenerator::nativeTypeName(const Type& type,
                                                   const char* prefix) const {
  std::string result = prefix;
  for (char character : type.declaration)
    result += std::isalnum(static_cast<unsigned char>(character)) || character == '_'
                  ? character
                  : '_';
  return result;
}

std::string BootstrapCodeGenerator::cppType(Type type) const {
  switch (type.kind) {
  case TypeKind::Int: return "std::int64_t";
  case TypeKind::Float: return "double";
  case TypeKind::Bool: return "bool";
  case TypeKind::Char: return "char";
  case TypeKind::String: return "std::string";
  case TypeKind::Unit: return "RocketUnit";
  case TypeKind::Array:
    return "RocketArray<" + cppType(collectionElementType(type)) + ">";
  case TypeKind::Slice:
    return "RocketSlice<" + cppType(collectionElementType(type)) + ">";
  case TypeKind::Weak:
    return "RocketWeak<" + cppType(type.arguments.at(0)) + ">";
  case TypeKind::UniqueBuffer:
    return "RocketUniqueBuffer<" + cppType(type.arguments.at(0)) + ">";
  case TypeKind::Task:
    return "RocketTask";
  case TypeKind::Pointer:
    if (type.arguments.empty() || type.arguments[0] == Type::Unit) return "void*";
    return nativeCppType(type.arguments[0]) + "*";
  case TypeKind::Struct:
    if (type.declaration == "std.string.Builder") return "RocketStringBuilder";
    if (type.declaration == "std.cancel.CancellationToken") return "RocketCancellation";
    if (type.declaration == "std.sync.Event") return "RocketEvent";
    if (type.declaration == "std.sync.AtomicInt") return "RocketAtomicInt";
    if (type.declaration == "std.sync.Mutex")
      return "RocketMutex<" + cppType(type.arguments.at(0)) + ">";
    if (type.declaration == "std.sync.LockGuard")
      return "RocketLockGuard<" + cppType(type.arguments.at(0)) + ">";
    if (type.declaration == "std.sync.Once")
      return "RocketOnce<" + cppType(type.arguments.at(0)) + ">";
    if (type.declaration == "std.task.TaskGroup")
      return "RocketTaskGroup<" + cppType(type.arguments.at(0)) + ">";
    if (type.declaration == "std.thread.Thread")
      return "RocketThread<" + cppType(type.arguments.at(0)) + ">";
    if (type.declaration == "std.channel.Channel")
      return "RocketChannel<" + cppType(type.arguments.at(0)) + ">";
    if (type.declaration == "std.channel.Sender")
      return "RocketSender<" + cppType(type.arguments.at(0)) + ">";
    if (type.declaration == "std.channel.Receiver")
      return "RocketReceiver<" + cppType(type.arguments.at(0)) + ">";
    return "RocketAggregate";
  case TypeKind::Enum: return "RocketAggregate";
  case TypeKind::NativeStruct: return nativeTypeName(type, "RocketNative_");
  case TypeKind::Opaque: return nativeTypeName(type, "RocketOpaque_") + "*";
  case TypeKind::Callback: return nativeTypeName(type, "RocketCallback_");
  case TypeKind::TypeParameter:
  case TypeKind::Invalid: break;
  }
  return "void";
}

std::string BootstrapCodeGenerator::nativeCppType(Type type) const {
  if (type == Type::Bool || type == Type::Char) return "std::uint8_t";
  if (type == Type::Int) return "std::int64_t";
  if (type == Type::Float) return "double";
  if (type == Type::Unit) return "void";
  return cppType(std::move(type));
}

std::string BootstrapCodeGenerator::functionName(SymbolId symbol) const {
  std::string name = module_.symbols.at(symbol).name;
  for (char& character : name)
    if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
      character = '_';
  return "rocket_fn_" + name + "_" + std::to_string(symbol);
}

std::string BootstrapCodeGenerator::localName(MirLocalId local) {
  return "rocket_l_" + std::to_string(local);
}

std::string BootstrapCodeGenerator::escaped(const std::string& text) {
  std::string result;
  for (char c : text) {
    switch (c) {
    case '\\': result += "\\\\"; break;
    case '"': result += "\\\""; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default: result.push_back(c); break;
    }
  }
  return result;
}

std::string BootstrapCodeGenerator::escapedCharacter(const std::string& text) {
  if (text == "\\") return "\\\\";
  if (text == "'") return "\\'";
  if (text == "\n") return "\\n";
  if (text == "\r") return "\\r";
  if (text == "\t") return "\\t";
  return text;
}

const char* BootstrapCodeGenerator::standardFunctionName(Intrinsic intrinsic) {
  switch (intrinsic) {
  case Intrinsic::StringByteLength: return "rocket_std_string_byte_length";
  case Intrinsic::StringConcat: return "rocket_std_string_concat";
  case Intrinsic::StringContains: return "rocket_std_string_contains";
  case Intrinsic::StringStartsWith: return "rocket_std_string_starts_with";
  case Intrinsic::StringEndsWith: return "rocket_std_string_ends_with";
  case Intrinsic::StringTrim: return "rocket_std_string_trim";
  case Intrinsic::StringSplit: return "rocket_std_string_split";
  case Intrinsic::StringByteAt: return "rocket_std_string_byte_at";
  case Intrinsic::StringByteValueAt: return "rocket_std_string_byte_value_at";
  case Intrinsic::StringSlice: return "rocket_std_string_slice";
  case Intrinsic::StringParseInt: return "rocket_std_string_parse_int";
  case Intrinsic::StringFromInt: return "rocket_std_string_from_int";
  case Intrinsic::StringBuilderNew: return "rocket_std_string_builder";
  case Intrinsic::StringBuilderAppend: return "rocket_std_string_builder_append";
  case Intrinsic::StringBuilderFinish: return "rocket_std_string_builder_finish";
  case Intrinsic::CollectionsLength: return "rocket_std_collections_length";
  case Intrinsic::CollectionsCapacity: return "rocket_std_collections_capacity";
  case Intrinsic::CollectionsReserve: return "rocket_std_collections_reserve";
  case Intrinsic::CollectionsAppend: return "rocket_std_collections_append";
  case Intrinsic::CollectionsPop: return "rocket_std_collections_pop";
  case Intrinsic::CollectionsInsert: return "rocket_std_collections_insert";
  case Intrinsic::CollectionsRemove: return "rocket_std_collections_remove";
  case Intrinsic::CollectionsClear: return "rocket_std_collections_clear";
  case Intrinsic::CollectionsMapFromArrays: return "rocket_std_collections_map_from_arrays";
  case Intrinsic::CollectionsMapLength: return "rocket_std_collections_map_length";
  case Intrinsic::CollectionsMapFind: return "rocket_std_collections_map_find";
  case Intrinsic::CollectionsMapGet: return "rocket_std_collections_map_get";
  case Intrinsic::CollectionsMapKeys: return "rocket_std_collections_map_keys";
  case Intrinsic::CollectionsMapValues: return "rocket_std_collections_map_values";
  case Intrinsic::CollectionsSetFromArray: return "rocket_std_collections_set_from_array";
  case Intrinsic::CollectionsSetContains: return "rocket_std_collections_set_contains";
  case Intrinsic::CollectionsSetValues: return "rocket_std_collections_set_values";
  case Intrinsic::CollectionsHash: return "rocket_std_collections_hash";
  case Intrinsic::CollectionsContains: return "rocket_std_collections_contains";
  case Intrinsic::CollectionsFind: return "rocket_std_collections_find";
  case Intrinsic::CollectionsFilterEqual: return "rocket_std_collections_filter_equal";
  case Intrinsic::CollectionsSortInt: return "rocket_std_collections_sort_int";
  case Intrinsic::CollectionsSortFloat: return "rocket_std_collections_sort_float";
  case Intrinsic::CollectionsSortChar: return "rocket_std_collections_sort_char";
  case Intrinsic::CollectionsSortString: return "rocket_std_collections_sort_string";
  case Intrinsic::CollectionsMapHash: return "rocket_std_collections_map_hash";
  case Intrinsic::CollectionsFoldSumInt: return "rocket_std_collections_fold_sum_int";
  case Intrinsic::CollectionsFoldSumFloat: return "rocket_std_collections_fold_sum_float";
  case Intrinsic::CollectionsReverse: return "rocket_std_collections_reverse";
  case Intrinsic::CollectionsConcat: return "rocket_std_collections_concat";
  case Intrinsic::CollectionsJoin: return "rocket_std_collections_join";
  case Intrinsic::FileReadText: return "rocket_std_file_read_text";
  case Intrinsic::FileWriteText: return "rocket_std_file_write_text";
  case Intrinsic::FileAppendText: return "rocket_std_file_append_text";
  case Intrinsic::FileExists: return "rocket_std_file_exists";
  case Intrinsic::FileRemove: return "rocket_std_file_remove";
  case Intrinsic::FileList: return "rocket_std_file_list";
  case Intrinsic::FileCreateDirectory: return "rocket_std_file_create_directory";
  case Intrinsic::FileReadBinary: return "rocket_std_file_read_binary";
  case Intrinsic::FileWriteBinary: return "rocket_std_file_write_binary";
  case Intrinsic::FileAppendBinary: return "rocket_std_file_append_binary";
  case Intrinsic::BinaryFromString: return "rocket_std_binary_from_string";
  case Intrinsic::BinaryToString: return "rocket_std_binary_to_string";
  case Intrinsic::BinaryLength: return "rocket_std_binary_length";
  case Intrinsic::BinarySlice: return "rocket_std_binary_slice";
  case Intrinsic::BinaryReadU8: return "rocket_std_binary_read_u8";
  case Intrinsic::BinaryReadU16Le: return "rocket_std_binary_read_u16_le";
  case Intrinsic::BinaryReadU32Le: return "rocket_std_binary_read_u32_le";
  case Intrinsic::BinaryWriteU8: return "rocket_std_binary_write_u8";
  case Intrinsic::BinaryWriteU16Le: return "rocket_std_binary_write_u16_le";
  case Intrinsic::BinaryWriteU32Le: return "rocket_std_binary_write_u32_le";
  case Intrinsic::BinaryConcat: return "rocket_std_binary_concat";
  case Intrinsic::BinaryReadU16Be: return "rocket_std_binary_read_u16_be";
  case Intrinsic::BinaryReadU32Be: return "rocket_std_binary_read_u32_be";
  case Intrinsic::BinaryWriteU16Be: return "rocket_std_binary_write_u16_be";
  case Intrinsic::BinaryWriteU32Be: return "rocket_std_binary_write_u32_be";
  case Intrinsic::StreamOpenReader: return "rocket_std_stream_open_reader";
  case Intrinsic::StreamRead: return "rocket_std_stream_read";
  case Intrinsic::StreamCloseReader: return "rocket_std_stream_close_reader";
  case Intrinsic::StreamOpenWriter: return "rocket_std_stream_open_writer";
  case Intrinsic::StreamWrite: return "rocket_std_stream_write";
  case Intrinsic::StreamFlush: return "rocket_std_stream_flush";
  case Intrinsic::StreamCloseWriter: return "rocket_std_stream_close_writer";
  case Intrinsic::UnicodeScalarCount: return "rocket_std_unicode_scalar_count";
  case Intrinsic::UnicodeScalarAt: return "rocket_std_unicode_scalar_at";
  case Intrinsic::UnicodeFromScalar: return "rocket_std_unicode_from_scalar";
  case Intrinsic::UnicodeNormalizeNfc: return "rocket_std_unicode_normalize_nfc";
  case Intrinsic::UnicodeNormalizeNfd: return "rocket_std_unicode_normalize_nfd";
  case Intrinsic::UnicodeGraphemeCount: return "rocket_std_unicode_grapheme_count";
  case Intrinsic::UnicodeGraphemeAt: return "rocket_std_unicode_grapheme_at";
  case Intrinsic::RegexIsMatch: return "rocket_std_regex_is_match";
  case Intrinsic::RegexFindAll: return "rocket_std_regex_find_all";
  case Intrinsic::RegexReplaceAll: return "rocket_std_regex_replace_all";
  case Intrinsic::CryptoSecureBytes: return "rocket_std_crypto_secure_bytes";
  case Intrinsic::CryptoSecureInt: return "rocket_std_crypto_secure_int";
  case Intrinsic::CryptoSha256: return "rocket_std_crypto_sha256";
  case Intrinsic::CryptoHmacSha256: return "rocket_std_crypto_hmac_sha256";
  case Intrinsic::CryptoConstantTimeEqual: return "rocket_std_crypto_constant_time_equal";
  case Intrinsic::CryptoVerifySignedFile: return "rocket_std_crypto_verify_signed_file";
  case Intrinsic::NetResolve: return "rocket_std_net_resolve";
  case Intrinsic::NetTcpConnect: return "rocket_std_net_tcp_connect";
  case Intrinsic::NetTcpListen: return "rocket_std_net_tcp_listen";
  case Intrinsic::NetAccept: return "rocket_std_net_accept";
  case Intrinsic::NetSend: return "rocket_std_net_send";
  case Intrinsic::NetReceive: return "rocket_std_net_receive";
  case Intrinsic::NetClose: return "rocket_std_net_close";
  case Intrinsic::NetCancel: return "rocket_std_net_cancel";
  case Intrinsic::NetLocalPort: return "rocket_std_net_local_port";
  case Intrinsic::HttpRequest: return "rocket_std_http_request";
  case Intrinsic::HttpReadRequest: return "rocket_std_http_read_request";
  case Intrinsic::HttpWriteResponse: return "rocket_std_http_write_response";
  case Intrinsic::DateTimeFormatUtc: return "rocket_std_datetime_format_utc";
  case Intrinsic::DateTimeParseUtc: return "rocket_std_datetime_parse_utc";
  case Intrinsic::DateTimeDaysInMonth: return "rocket_std_datetime_days_in_month";
  case Intrinsic::DateTimeWeekday: return "rocket_std_datetime_weekday";
  case Intrinsic::DateTimeLocalOffsetMinutes: return "rocket_std_datetime_local_offset_minutes";
  case Intrinsic::DateTimeTimezoneName: return "rocket_std_datetime_timezone_name";
  case Intrinsic::LogWrite: return "rocket_std_log_write";
  case Intrinsic::LogAppend: return "rocket_std_log_append";
  case Intrinsic::CliHasFlag: return "rocket_std_cli_has_flag";
  case Intrinsic::CliOption: return "rocket_std_cli_option";
  case Intrinsic::CliPositionals: return "rocket_std_cli_positionals";
  case Intrinsic::ConfigGet: return "rocket_std_config_get";
  case Intrinsic::ConfigLoad: return "rocket_std_config_load";
  case Intrinsic::CompressionXpressCompress: return "rocket_std_compression_xpress_compress";
  case Intrinsic::CompressionXpressDecompress: return "rocket_std_compression_xpress_decompress";
  case Intrinsic::ArchiveTarCreate: return "rocket_std_archive_tar_create";
  case Intrinsic::ArchiveTarList: return "rocket_std_archive_tar_list";
  case Intrinsic::ArchiveTarRead: return "rocket_std_archive_tar_read";
  case Intrinsic::SqliteOpen: return "rocket_std_sqlite_open";
  case Intrinsic::SqliteExecute: return "rocket_std_sqlite_execute";
  case Intrinsic::SqliteQuery: return "rocket_std_sqlite_query";
  case Intrinsic::SqliteClose: return "rocket_std_sqlite_close";
  case Intrinsic::TestingAssert: return "rocket_std_testing_assert";
  case Intrinsic::TestingEqualInt: return "rocket_std_testing_equal_int";
  case Intrinsic::TestingEqualString: return "rocket_std_testing_equal_string";
  case Intrinsic::TestingTempDirectory: return "rocket_std_testing_temp_directory";
  case Intrinsic::TestingFixturePath: return "rocket_std_testing_fixture_path";
  case Intrinsic::TestingCleanupTemp: return "rocket_std_testing_cleanup_temp";
  case Intrinsic::TestingCoverageHit: return "rocket_std_testing_coverage_hit";
  case Intrinsic::TestingCoverageWrite: return "rocket_std_testing_coverage_write";
  case Intrinsic::PathJoin: return "rocket_std_path_join";
  case Intrinsic::PathBasename: return "rocket_std_path_basename";
  case Intrinsic::PathExtension: return "rocket_std_path_extension";
  case Intrinsic::PathNormalize: return "rocket_std_path_normalize";
  case Intrinsic::JsonParse: return "rocket_std_json_parse";
  case Intrinsic::JsonStringify: return "rocket_std_json_stringify";
  case Intrinsic::CsvParse: return "rocket_std_csv_parse";
  case Intrinsic::CsvEncode: return "rocket_std_csv_encode";
  case Intrinsic::RandomSeed: return "rocket_std_random_seed";
  case Intrinsic::RandomInt: return "rocket_std_random_int";
  case Intrinsic::RandomFloat: return "rocket_std_random_float";
  case Intrinsic::ProcessRun: return "rocket_std_process_run";
  case Intrinsic::ProcessArguments: return "rocket_std_process_arguments";
  case Intrinsic::ProcessExecutablePath: return "rocket_std_process_executable_path";
  case Intrinsic::ProcessEnvironment: return "rocket_std_process_environment";
  case Intrinsic::ProcessWorkingDirectory: return "rocket_std_process_working_directory";
  case Intrinsic::TimeUnixMilliseconds: return "rocket_std_time_unix_milliseconds";
  case Intrinsic::TimeMonotonicMilliseconds: return "rocket_std_time_monotonic_milliseconds";
  case Intrinsic::TimeSleepMilliseconds: return "rocket_std_time_sleep_milliseconds";
  case Intrinsic::TaskJoin: return "rocket_std_task_join";
  case Intrinsic::TaskIsComplete: return "rocket_std_task_is_complete";
  case Intrinsic::OwnershipDowngrade: return "rocket_std_ownership_downgrade";
  case Intrinsic::OwnershipUpgrade: return "rocket_std_ownership_upgrade";
  case Intrinsic::OwnershipExpired: return "rocket_std_ownership_expired";
  case Intrinsic::BufferThaw: return "rocket_std_buffer_thaw";
  case Intrinsic::BufferLength: return "rocket_std_buffer_length";
  case Intrinsic::BufferCapacity: return "rocket_std_buffer_capacity";
  case Intrinsic::BufferGet: return "rocket_std_buffer_get";
  case Intrinsic::BufferSet: return "rocket_std_buffer_set";
  case Intrinsic::BufferAppend: return "rocket_std_buffer_append";
  case Intrinsic::BufferSlice: return "rocket_std_buffer_slice";
  case Intrinsic::BufferFreeze: return "rocket_std_buffer_freeze";
  case Intrinsic::CancelToken: return "rocket_std_cancel_token";
  case Intrinsic::CancelChild: return "rocket_std_cancel_child";
  case Intrinsic::CancelCurrent: return "rocket_std_cancel_current";
  case Intrinsic::CancelCancel: return "rocket_std_cancel_cancel";
  case Intrinsic::CancelIsCancelled: return "rocket_std_cancel_is_cancelled";
  case Intrinsic::CancelCheck: return "rocket_std_cancel_check";
  case Intrinsic::AsyncTimeDeadlineAfter: return "rocket_std_async_time_deadline_after";
  case Intrinsic::AsyncTimeRemaining: return "rocket_std_async_time_remaining";
  case Intrinsic::AsyncTimeSleep: return "rocket_std_async_time_sleep";
  case Intrinsic::AsyncTimeSleepUntil: return "rocket_std_async_time_sleep_until";
  case Intrinsic::SyncMutex: return "rocket_std_sync_mutex";
  case Intrinsic::SyncLock: return "rocket_std_sync_lock";
  case Intrinsic::SyncGuardGet: return "rocket_std_sync_guard_get";
  case Intrinsic::SyncGuardSet: return "rocket_std_sync_guard_set";
  case Intrinsic::SyncUnlock: return "rocket_std_sync_unlock";
  case Intrinsic::SyncEvent: return "rocket_std_sync_event";
  case Intrinsic::SyncEventSet: return "rocket_std_sync_event_set";
  case Intrinsic::SyncEventReset: return "rocket_std_sync_event_reset";
  case Intrinsic::SyncEventWait: return "rocket_std_sync_event_wait";
  case Intrinsic::SyncAtomicInt: return "rocket_std_sync_atomic_int";
  case Intrinsic::SyncAtomicLoad: return "rocket_std_sync_atomic_load";
  case Intrinsic::SyncAtomicStore: return "rocket_std_sync_atomic_store";
  case Intrinsic::SyncAtomicFetchAdd: return "rocket_std_sync_atomic_fetch_add";
  case Intrinsic::SyncAtomicCompareExchange: return "rocket_std_sync_atomic_compare_exchange";
  case Intrinsic::SyncOnce: return "rocket_std_sync_once";
  case Intrinsic::SyncOnceSet: return "rocket_std_sync_once_set";
  case Intrinsic::SyncOnceGet: return "rocket_std_sync_once_get";
  case Intrinsic::ChannelBounded: return "rocket_std_channel_bounded";
  case Intrinsic::ChannelUnbounded: return "rocket_std_channel_unbounded";
  case Intrinsic::ChannelSender: return "rocket_std_channel_sender";
  case Intrinsic::ChannelReceiver: return "rocket_std_channel_receiver";
  case Intrinsic::ChannelCloneSender: return "rocket_std_channel_clone_sender";
  case Intrinsic::ChannelCloneReceiver: return "rocket_std_channel_clone_receiver";
  case Intrinsic::ChannelSend: return "rocket_std_channel_send";
  case Intrinsic::ChannelReceive: return "rocket_std_channel_receive";
  case Intrinsic::ChannelCloseSender: return "rocket_std_channel_close_sender";
  case Intrinsic::ChannelCloseReceiver: return "rocket_std_channel_close_receiver";
  case Intrinsic::AsyncFileRead: return "rocket_std_async_file_read";
  case Intrinsic::AsyncFileWrite: return "rocket_std_async_file_write";
  case Intrinsic::TaskGroup: return "rocket_std_task_group";
  case Intrinsic::TaskGroupJoin: return "rocket_std_task_group_join";
  case Intrinsic::AsyncNetConnect: return "rocket_std_async_net_connect";
  case Intrinsic::AsyncNetAccept: return "rocket_std_async_net_accept";
  case Intrinsic::AsyncNetReceive: return "rocket_std_async_net_receive";
  case Intrinsic::AsyncNetSend: return "rocket_std_async_net_send";
  case Intrinsic::AsyncProcessRun: return "rocket_std_async_process_run";
  case Intrinsic::ThreadSpawn: return "rocket_std_thread_spawn";
  case Intrinsic::ThreadJoin: return "rocket_std_thread_join";
  case Intrinsic::ThreadDetach: return "rocket_std_thread_detach";
  case Intrinsic::ThreadIsComplete: return "rocket_std_thread_is_complete";
  case Intrinsic::TaskCancel: return "rocket_std_task_cancel";
  case Intrinsic::TaskGroupCancel: return "rocket_std_task_group_cancel";
  case Intrinsic::SyncOnceEmpty: return "rocket_std_sync_once_empty";
  case Intrinsic::TargetAlias: return "rocket_std_target_alias";
  case Intrinsic::TargetTriple: return "rocket_std_target_triple";
  case Intrinsic::TargetOs: return "rocket_std_target_os";
  case Intrinsic::TargetArchitecture: return "rocket_std_target_architecture";
  case Intrinsic::TargetEnvironment: return "rocket_std_target_environment";
  case Intrinsic::TargetPointerWidth: return "rocket_std_target_pointer_width";
  case Intrinsic::TargetEndianness: return "rocket_std_target_endianness";
  case Intrinsic::TargetHasFeature: return "rocket_std_target_has_feature";
  default: return nullptr;
  }
}

std::string BootstrapCodeGenerator::generate() const {
  std::ostringstream out;
  out << "// Generated by rocketc bootstrap backend from verified MIR. Do not edit.\n"
         "#include <any>\n#include <atomic>\n#include <chrono>\n#include <condition_variable>\n"
         "#include <cstdint>\n#include <cstdlib>\n#include <deque>\n#include <functional>\n"
         "#include <initializer_list>\n#include <future>\n#include <iostream>\n#include <limits>\n"
         "#include <memory>\n#include <mutex>\n#include <stdexcept>\n#include <string>\n#include <thread>\n"
         "#include <utility>\n#include <vector>\n\n"
         "struct RocketUnit {};\n"
         "constexpr bool operator==(RocketUnit, RocketUnit) { return true; }\n"
         "template <typename T> RocketUnit rocket_print(const T& value) { "
         "std::cout << value << '\\n'; return {}; }\n"
         "template <typename T> using RocketArray = std::shared_ptr<std::vector<T>>;\n"
         "template <typename T> struct RocketWeak { std::weak_ptr<typename T::element_type> value; };\n"
         "template <typename T> using RocketUniqueBuffer = std::shared_ptr<std::vector<T>>;\n"
         "template <typename T> struct RocketSlice { RocketArray<T> owner; "
         "std::int64_t offset{}; std::int64_t length{}; };\n"
         "struct RocketAggregateData { std::uint32_t tag{}; std::vector<std::any> fields; };\n"
         "using RocketAggregate = std::shared_ptr<RocketAggregateData>;\n"
         "struct RocketCancellationState { std::atomic<bool> cancelled{false}; "
         "std::shared_ptr<RocketCancellationState> parent; };\n"
         "using RocketCancellation = std::shared_ptr<RocketCancellationState>;\n"
         "inline thread_local RocketCancellation rocket_stage0_current_cancellation;\n"
         "inline bool rocket_stage0_token_cancelled(const RocketCancellation& token) { "
         "for (auto current = token; current; current = current->parent) "
         "if (current->cancelled.load(std::memory_order_acquire)) return true; return false; }\n"
         "struct RocketTaskState { std::shared_future<RocketAggregate> future; "
         "RocketCancellation cancellation; RocketTaskState(std::shared_future<RocketAggregate> f, "
         "RocketCancellation c) : future(std::move(f)), cancellation(std::move(c)) {} };\n"
         "using RocketTask = std::shared_ptr<RocketTaskState>;\n"
         "class RocketStage0Executor { public: RocketStage0Executor() { "
         "unsigned detected = std::thread::hardware_concurrency(); unsigned count = "
         "(std::max)(1U, (std::min)(detected == 0 ? 4U : detected, 64U)); "
         "for (unsigned index = 0; index < count; ++index) workers_.emplace_back([this] { run(); }); } "
         "~RocketStage0Executor() { { std::lock_guard lock(mutex_); stopping_ = true; } "
         "ready_.notify_all(); space_.notify_all(); for (auto& worker : workers_) "
         "if (worker.joinable()) worker.join(); } "
         "void enqueue(std::function<void()> work) { std::unique_lock lock(mutex_); "
         "while (!stopping_ && queue_.size() >= 65536) { if (worker_) { lock.unlock(); "
         "if (!help_one()) std::this_thread::yield(); lock.lock(); } else space_.wait(lock); } "
         "if (stopping_) throw std::runtime_error(\"task executor is shutting down\"); "
         "queue_.push_back(std::move(work)); lock.unlock(); ready_.notify_one(); } "
         "bool help_one() { std::function<void()> work; { std::lock_guard lock(mutex_); "
         "if (queue_.empty()) return false; work = std::move(queue_.front()); queue_.pop_front(); } "
         "space_.notify_one(); work(); return true; } static bool is_worker() { return worker_; } "
         "private: void run() { worker_ = true; while (true) { std::function<void()> work; "
         "{ std::unique_lock lock(mutex_); ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); }); "
         "if (stopping_ && queue_.empty()) break; work = std::move(queue_.front()); queue_.pop_front(); } "
         "space_.notify_one(); work(); } worker_ = false; } std::mutex mutex_; "
         "std::condition_variable ready_, space_; std::deque<std::function<void()>> queue_; "
         "std::vector<std::thread> workers_; bool stopping_ = false; "
         "inline static thread_local bool worker_ = false; };\n"
         "inline RocketStage0Executor& rocket_stage0_executor() { static RocketStage0Executor value; return value; }\n"
         "template <typename F> RocketTask rocket_task(F body) { "
         "auto cancellation = std::make_shared<RocketCancellationState>(); "
         "auto promise = std::make_shared<std::promise<RocketAggregate>>(); "
         "auto future = promise->get_future().share(); "
         "auto task = std::make_shared<RocketTaskState>(future, cancellation); "
         "rocket_stage0_executor().enqueue([body = std::move(body), cancellation, promise]() mutable { "
         "auto previous = rocket_stage0_current_cancellation; rocket_stage0_current_cancellation = cancellation; "
         "RocketAggregate result; if (rocket_stage0_token_cancelled(cancellation)) "
         "result = std::make_shared<RocketAggregateData>(RocketAggregateData{1, {std::string(\"operation cancelled\")}}); "
         "else { try { result = body(); } catch (const std::exception& error) { "
         "result = std::make_shared<RocketAggregateData>(RocketAggregateData{1, {std::string(error.what())}}); } } "
         "rocket_stage0_current_cancellation = previous; promise->set_value(std::move(result)); }); return task; }\n"
         "inline RocketAggregate rocket_await(const RocketTask& task) { while (task->future.wait_for("
         "std::chrono::milliseconds(0)) != std::future_status::ready) { if (RocketStage0Executor::is_worker()) { "
         "if (rocket_stage0_token_cancelled(rocket_stage0_current_cancellation)) { "
         "task->cancellation->cancelled.store(true, std::memory_order_release); return "
         "std::make_shared<RocketAggregateData>(RocketAggregateData{1, {std::string(\"operation cancelled\")}}); } "
         "if (!rocket_stage0_executor().help_one()) std::this_thread::yield(); } else task->future.wait(); } "
         "return task->future.get(); }\n"
         "inline RocketAggregate rocket_aggregate(std::uint32_t tag, std::vector<std::any> fields) { "
         "return std::make_shared<RocketAggregateData>(RocketAggregateData{tag, std::move(fields)}); }\n"
         "template <typename T> T rocket_field(const RocketAggregate& value, std::size_t index) { "
         "return std::any_cast<T>(value->fields.at(index)); }\n"
         "inline std::int64_t rocket_tag(const RocketAggregate& value) { return value->tag; }\n"
         "[[noreturn]] inline void rocket_bounds_error() { "
         "std::cerr << \"rocket runtime error: collection bounds failure\\n\"; std::exit(101); }\n"
         "[[noreturn]] inline void rocket_integer_error(const char* message) { "
         "std::cerr << \"rocket runtime error: \" << message << '\\n'; std::exit(101); }\n"
         "inline std::int64_t rocket_int_add(std::int64_t left, std::int64_t right) { "
         "if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) || "
         "(right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left + right; }\n"
         "inline std::int64_t rocket_int_sub(std::int64_t left, std::int64_t right) { "
         "if ((right < 0 && left > std::numeric_limits<std::int64_t>::max() + right) || "
         "(right > 0 && left < std::numeric_limits<std::int64_t>::min() + right)) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left - right; }\n"
         "inline std::int64_t rocket_int_mul(std::int64_t left, std::int64_t right) { "
         "if (left == 0 || right == 0) return 0; "
         "if ((left == -1 && right == std::numeric_limits<std::int64_t>::min()) || "
         "(right == -1 && left == std::numeric_limits<std::int64_t>::min())) "
         "rocket_integer_error(\"Int arithmetic overflow\"); "
         "if ((left > 0 && right > 0 && left > std::numeric_limits<std::int64_t>::max() / right) || "
         "(left > 0 && right < 0 && right < std::numeric_limits<std::int64_t>::min() / left) || "
         "(left < 0 && right > 0 && left < std::numeric_limits<std::int64_t>::min() / right) || "
         "(left < 0 && right < 0 && right < std::numeric_limits<std::int64_t>::max() / left)) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left * right; }\n"
         "inline std::int64_t rocket_int_div(std::int64_t left, std::int64_t right) { "
         "if (right == 0) rocket_integer_error(\"Int division by zero\"); "
         "if (left == std::numeric_limits<std::int64_t>::min() && right == -1) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left / right; }\n"
         "template <typename T> RocketArray<T> rocket_array(std::initializer_list<T> values) { "
         "return std::make_shared<std::vector<T>>(values); }\n"
         "template <typename T> RocketArray<T> rocket_array_clone(const RocketArray<T>& values, "
         "std::size_t capacity) { auto result = std::make_shared<std::vector<T>>(); "
         "result->reserve(capacity); result->insert(result->end(), values->begin(), values->end()); "
         "return result; }\n"
         "template <typename T> RocketArray<T> rocket_array_update(const RocketArray<T>& values, "
         "std::int64_t index, T value) { "
         "if (index < 0 || index >= static_cast<std::int64_t>(values->size())) rocket_bounds_error(); "
         "RocketArray<T> result = values.use_count() == 1 ? values : rocket_array_clone(values, values->capacity()); "
         "(*result)[static_cast<std::size_t>(index)] = std::move(value); return result; }\n"
         "template <typename T> T rocket_index(const RocketArray<T>& values, std::int64_t index) { "
         "if (index < 0 || index >= static_cast<std::int64_t>(values->size())) rocket_bounds_error(); "
         "return (*values)[static_cast<std::size_t>(index)]; }\n"
         "template <typename T> T rocket_index(const RocketSlice<T>& values, std::int64_t index) { "
         "if (index < 0 || index >= values.length) rocket_bounds_error(); "
         "return (*values.owner)[static_cast<std::size_t>(values.offset + index)]; }\n"
         "template <typename T> RocketSlice<T> rocket_slice(const RocketArray<T>& values, "
         "std::int64_t start, std::int64_t end) { "
         "if (start < 0 || end < start || end > static_cast<std::int64_t>(values->size())) "
         "rocket_bounds_error(); return {values, start, end - start}; }\n"
         "template <typename T> RocketSlice<T> rocket_slice(const RocketSlice<T>& values, "
         "std::int64_t start, std::int64_t end) { "
         "if (start < 0 || end < start || end > values.length) rocket_bounds_error(); "
         "return {values.owner, values.offset + start, end - start}; }\n"
         "#include \"stage0_stdlib.h\"\n"
         "#if defined(_WIN32)\n"
         "#define ROCKET_EXPORT __declspec(dllexport)\n"
         "#else\n"
         "#define ROCKET_EXPORT __attribute__((visibility(\"default\")))\n"
         "#endif\n\n";
  for (const auto& declaration : module_.typeDeclarations) {
    const Type type{declaration.kind == HirTypeDeclKind::NativeStruct
                        ? TypeKind::NativeStruct
                        : declaration.kind == HirTypeDeclKind::Opaque
                              ? TypeKind::Opaque
                              : declaration.kind == HirTypeDeclKind::Callback
                                    ? TypeKind::Callback
                                    : TypeKind::Invalid,
                    declaration.name};
    if (declaration.kind == HirTypeDeclKind::NativeStruct) {
      out << "struct " << nativeTypeName(type, "RocketNative_") << " { ";
      for (const auto& field : declaration.fields)
        out << nativeCppType(field.type) << ' ' << field.name << "; ";
      out << "};\n";
    } else if (declaration.kind == HirTypeDeclKind::Opaque) {
      out << "struct " << nativeTypeName(type, "RocketOpaque_") << ";\n";
    } else if (declaration.kind == HirTypeDeclKind::Callback) {
      out << "using " << nativeTypeName(type, "RocketCallback_") << " = "
          << nativeCppType(declaration.callbackResult) << "(*)" << '(';
      for (std::size_t index = 0; index < declaration.callbackParameters.size(); ++index) {
        if (index) out << ", ";
        out << nativeCppType(declaration.callbackParameters[index]);
      }
      out << ");\n";
    }
  }
  out << '\n';
  for (const auto& symbol : module_.symbols) {
    if (!symbol.nativeImport) continue;
    out << "extern \"C\" " << nativeCppType(symbol.type) << ' ' << symbol.nativeName
        << '(';
    for (std::size_t index = 0; index < symbol.parameterTypes.size(); ++index) {
      if (index) out << ", ";
      out << nativeCppType(symbol.parameterTypes[index]);
    }
    out << ");\n";
  }
  out << '\n';
  for (const auto& function : module_.functions) {
    out << cppType(function.result) << ' ' << functionName(function.symbol) << '(';
    for (std::size_t i = 0; i < function.parameters.size(); ++i) {
      if (i) out << ", ";
      const MirLocalId parameter = function.parameters[i];
      out << cppType(function.locals[parameter].type) << ' ' << localName(parameter);
    }
    out << ");\n";
  }
  for (const auto& function : module_.functions) {
    const auto& symbol = module_.symbols[function.symbol];
    bool compatible = isNativeAbiValueType(symbol.type) &&
                      symbol.type.kind != TypeKind::Callback;
    for (const auto& parameter : symbol.parameterTypes)
      compatible = compatible && isNativeAbiValueType(parameter) &&
                   parameter != Type::Unit && parameter.kind != TypeKind::Callback;
    if (!compatible) continue;
    out << nativeCppType(symbol.type) << " rocket_callback_" << symbol.id << '(';
    for (std::size_t index = 0; index < symbol.parameterTypes.size(); ++index) {
      if (index) out << ", ";
      out << nativeCppType(symbol.parameterTypes[index]) << " argument" << index;
    }
    out << ");\n";
  }
  out << '\n';
  for (const auto& function : module_.functions) emitFunction(out, function);

  for (const auto& function : module_.functions) {
    const auto& symbol = module_.symbols[function.symbol];
    bool compatible = isNativeAbiValueType(symbol.type) &&
                      symbol.type.kind != TypeKind::Callback;
    for (const auto& parameter : symbol.parameterTypes)
      compatible = compatible && isNativeAbiValueType(parameter) &&
                   parameter != Type::Unit && parameter.kind != TypeKind::Callback;
    if (!compatible) continue;
    out << nativeCppType(symbol.type) << " rocket_callback_" << symbol.id << '(';
    for (std::size_t index = 0; index < symbol.parameterTypes.size(); ++index) {
      if (index) out << ", ";
      out << nativeCppType(symbol.parameterTypes[index]) << " argument" << index;
    }
    out << ") { ";
    if (symbol.type != Type::Unit) out << "return static_cast<" << nativeCppType(symbol.type) << ">(";
    out << functionName(symbol.id) << '(';
    for (std::size_t index = 0; index < symbol.parameterTypes.size(); ++index) {
      if (index) out << ", ";
      if (symbol.parameterTypes[index] == Type::Bool)
        out << "argument" << index << " != 0";
      else
        out << "argument" << index;
    }
    out << ')';
    if (symbol.type != Type::Unit) out << ')';
    out << "; }\n";
  }

  for (const auto& symbol : module_.symbols) {
    if (!symbol.nativeExport) continue;
    out << "extern \"C\" ROCKET_EXPORT " << nativeCppType(symbol.type)
        << ' ' << symbol.nativeName << '(';
    for (std::size_t index = 0; index < symbol.parameterTypes.size(); ++index) {
      if (index) out << ", ";
      out << nativeCppType(symbol.parameterTypes[index]) << " argument" << index;
    }
    out << ") { ";
    if (symbol.type != Type::Unit) out << "return static_cast<" << nativeCppType(symbol.type) << ">(";
    out << functionName(symbol.id) << '(';
    for (std::size_t index = 0; index < symbol.parameterTypes.size(); ++index) {
      if (index) out << ", ";
      if (symbol.parameterTypes[index] == Type::Bool)
        out << "argument" << index << " != 0";
      else
        out << "argument" << index;
    }
    out << ')';
    if (symbol.type != Type::Unit) out << ')';
    out << "; }\n";
  }

  if (!module_.library) for (const auto& function : module_.functions) {
    if (module_.symbols[function.symbol].name == "main") {
      out << "int main(int argc, char** argv) { rocket_std_process_set_arguments(argc, argv); "
          << "return static_cast<int>(" << functionName(function.symbol) << "()); }\n";
      break;
    }
  }
  return out.str();
}

void BootstrapCodeGenerator::emitFunction(std::ostream& out,
                                          const MirFunction& function) const {
  out << cppType(function.result) << ' ' << functionName(function.symbol) << '(';
  for (std::size_t i = 0; i < function.parameters.size(); ++i) {
    if (i) out << ", ";
    const MirLocalId parameter = function.parameters[i];
    out << cppType(function.locals[parameter].type) << ' ' << localName(parameter);
  }
  out << ") {\n";
  for (MirLocalId local = 0; local < function.locals.size(); ++local) {
    if (!function.locals[local].parameter)
      out << "    " << cppType(function.locals[local].type) << ' ' << localName(local) << "{};\n";
  }
  out << "    goto rocket_bb_0;\n";
  for (std::size_t block = 0; block < function.blocks.size(); ++block) {
    out << "rocket_bb_" << block << ":\n";
    for (const auto& instruction : function.blocks[block].instructions)
      emitInstruction(out, instruction);
    emitTerminator(out, *function.blocks[block].terminator, function.result);
  }
  out << "}\n\n";
}

void BootstrapCodeGenerator::emitInstruction(std::ostream& out,
                                             const MirInstruction& instruction) const {
  if (instruction.kind != MirInstructionKind::Assign) {
    out << "    // bootstrap RAII: "
        << (instruction.kind == MirInstructionKind::Retain ? "retain " : "release ");
    emitOperand(out, instruction.arcOperand);
    out << "\n";
    return;
  }
  out << "    " << localName(instruction.destination) << " = ";
  emitRvalue(out, instruction.value);
  out << ";\n";
}

void BootstrapCodeGenerator::emitTerminator(std::ostream& out,
                                            const MirTerminator& terminator,
                                            Type functionResult) const {
  switch (terminator.kind) {
  case MirTerminatorKind::Goto:
    out << "    goto rocket_bb_" << terminator.target << ";\n";
    break;
  case MirTerminatorKind::Branch:
    out << "    if (";
    emitOperand(out, terminator.condition);
    out << ") goto rocket_bb_" << terminator.thenTarget << "; else goto rocket_bb_"
        << terminator.elseTarget << ";\n";
    break;
  case MirTerminatorKind::Return:
    out << "    return";
    if (terminator.returned.has_value()) {
      out << ' ';
      emitOperand(out, *terminator.returned);
    } else if (functionResult == Type::Unit) {
      out << " {}";
    }
    out << ";\n";
    break;
  }
}

void BootstrapCodeGenerator::emitRvalue(std::ostream& out, const MirRvalue& value) const {
  switch (value.kind) {
  case MirRvalueKind::Use:
    emitOperand(out, value.left);
    break;
  case MirRvalueKind::Unary:
    if (value.op == TokenKind::KwNot) {
      out << "(!";
      emitOperand(out, value.left);
      out << ')';
    } else if (value.type == Type::Int) {
      out << "rocket_int_sub(0, ";
      emitOperand(out, value.left);
      out << ')';
    } else {
      out << "(-";
      emitOperand(out, value.left);
      out << ')';
    }
    break;
  case MirRvalueKind::Binary: {
    if (value.left.type == Type::Int &&
        (value.op == TokenKind::Plus || value.op == TokenKind::Minus ||
         value.op == TokenKind::Star || value.op == TokenKind::Slash)) {
      const char* helper = value.op == TokenKind::Plus ? "rocket_int_add" :
                           value.op == TokenKind::Minus ? "rocket_int_sub" :
                           value.op == TokenKind::Star ? "rocket_int_mul" : "rocket_int_div";
      out << helper << '(';
      emitOperand(out, value.left);
      out << ", ";
      emitOperand(out, value.right);
      out << ')';
      break;
    }
    const char* op = value.op == TokenKind::KwAnd ? "&&" :
                     value.op == TokenKind::KwOr ? "||" : tokenName(value.op);
    out << '(';
    emitOperand(out, value.left);
    out << ' ' << op << ' ';
    emitOperand(out, value.right);
    out << ')';
    break;
  }
  case MirRvalueKind::Call: {
    bool nativeUnit = false;
    if (module_.symbols[value.callee].kind == SymbolKind::BuiltinFunction) {
      const HirSymbol& symbol = module_.symbols[value.callee];
      if (symbol.intrinsic == Intrinsic::Print) out << "rocket_print";
      else if (symbol.intrinsic == Intrinsic::Math)
        out << "rocket_std_math_" << symbol.name.substr(std::string("std.math.").size());
      else if (const char* name = standardFunctionName(symbol.intrinsic)) out << name;
      else out << "/* unknown standard-library intrinsic */";
      if (symbol.intrinsic == Intrinsic::CollectionsMapLength ||
          symbol.intrinsic == Intrinsic::CollectionsMapFind ||
          symbol.intrinsic == Intrinsic::CollectionsMapGet ||
          symbol.intrinsic == Intrinsic::CollectionsMapKeys ||
          symbol.intrinsic == Intrinsic::CollectionsMapValues) {
        const Type map = symbol.parameterTypes[0];
        out << '<' << cppType(map.arguments[0]) << ", " << cppType(map.arguments[1]) << '>';
      } else if (symbol.intrinsic == Intrinsic::CollectionsSetContains ||
                 symbol.intrinsic == Intrinsic::CollectionsSetValues) {
        const Type set = symbol.parameterTypes[0];
        out << '<' << cppType(set.arguments[0]) << '>';
      } else if (symbol.intrinsic == Intrinsic::TaskGroup) {
        out << '<' << cppType(value.type.arguments[0]) << '>';
      } else if (symbol.intrinsic == Intrinsic::ThreadSpawn) {
        out << '<' << cppType(symbol.parameterTypes[0].arguments[0]) << '>';
      }
    } else {
      const auto& symbol = module_.symbols[value.callee];
      nativeUnit = symbol.nativeImport && symbol.type == Type::Unit;
      if (nativeUnit) out << '(';
      out << (symbol.nativeImport ? symbol.nativeName : functionName(value.callee));
    }
    out << '(';
    for (std::size_t i = 0; i < value.arguments.size(); ++i) {
      if (i) out << ", ";
      emitOperand(out, value.arguments[i]);
    }
    out << ')';
    if (nativeUnit) out << ", RocketUnit{})";
    break;
  }
  case MirRvalueKind::AsyncCall:
    out << "rocket_task([=]() { return " << functionName(value.callee) << '(';
    for (std::size_t i = 0; i < value.arguments.size(); ++i) {
      if (i) out << ", ";
      emitOperand(out, value.arguments[i]);
    }
    out << "); })";
    break;
  case MirRvalueKind::Await:
    out << "rocket_await(";
    emitOperand(out, value.left);
    out << ')';
    break;
  case MirRvalueKind::Array:
    out << "rocket_array<" << cppType(collectionElementType(value.type)) << ">({";
    for (std::size_t i = 0; i < value.arguments.size(); ++i) {
      if (i) out << ", ";
      emitOperand(out, value.arguments[i]);
    }
    out << "})";
    break;
  case MirRvalueKind::ArrayUpdate:
    out << "rocket_array_update(";
    emitOperand(out, value.left);
    out << ", ";
    emitOperand(out, value.right);
    out << ", ";
    emitOperand(out, value.end);
    out << ')';
    break;
  case MirRvalueKind::Index:
    out << "rocket_index(";
    emitOperand(out, value.left);
    out << ", ";
    emitOperand(out, value.right);
    out << ')';
    break;
  case MirRvalueKind::Slice:
    out << "rocket_slice(";
    emitOperand(out, value.left);
    out << ", ";
    emitOperand(out, value.right);
    out << ", ";
    emitOperand(out, value.end);
    out << ')';
    break;
  case MirRvalueKind::Aggregate:
    out << "rocket_aggregate(" << value.tag << ", std::vector<std::any>{";
    for (std::size_t i = 0; i < value.arguments.size(); ++i) {
      if (i) out << ", ";
      emitOperand(out, value.arguments[i]);
    }
    out << "})";
    break;
  case MirRvalueKind::Field:
    out << "rocket_field<" << cppType(value.type) << ">(";
    emitOperand(out, value.left);
    out << ", " << value.tag << ')';
    break;
  case MirRvalueKind::Tag:
    out << "rocket_tag(";
    emitOperand(out, value.left);
    out << ')';
    break;
  }
}

void BootstrapCodeGenerator::emitOperand(std::ostream& out,
                                         const MirOperand& operand) const {
  if (operand.kind == MirOperandKind::Local) {
    out << localName(operand.local);
    return;
  }
  switch (operand.type.kind) {
  case TypeKind::Int:
    // std::int64_t is long on LP64 hosts and long long on Windows.  Preserve
    // the Rocket Int type in template deduction and std::any payloads instead
    // of letting the C++ literal spelling choose the host's long long type.
    out << "static_cast<std::int64_t>(" << operand.constant << ')';
    break;
  case TypeKind::Float: out << operand.constant; break;
  case TypeKind::Bool: out << operand.constant; break;
  case TypeKind::Char: out << '\'' << escapedCharacter(operand.constant) << '\''; break;
  case TypeKind::String: out << "std::string{\"" << escaped(operand.constant) << "\"}"; break;
  case TypeKind::Unit: out << "RocketUnit{}"; break;
  case TypeKind::Array:
  case TypeKind::Slice:
  case TypeKind::Weak:
  case TypeKind::UniqueBuffer:
  case TypeKind::Task:
  case TypeKind::Struct:
  case TypeKind::Enum:
  case TypeKind::Pointer:
  case TypeKind::NativeStruct:
  case TypeKind::Opaque:
    out << "/* invalid aggregate constant */";
    break;
  case TypeKind::Callback:
    out << "&rocket_callback_" << operand.constant;
    break;
  case TypeKind::TypeParameter:
  case TypeKind::Invalid: out << "/* invalid */"; break;
  }
}

} // namespace rocket
