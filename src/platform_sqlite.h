#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define ROCKET_SQLITE_CALL __cdecl
#else
#include <dlfcn.h>
#define ROCKET_SQLITE_CALL
#endif

struct sqlite3;
struct sqlite3_stmt;

namespace rocket::platform_sqlite {

inline bool validUtf8(std::string_view input) {
  for (std::size_t index = 0; index < input.size();) {
    const auto first = static_cast<std::uint8_t>(input[index]);
    std::size_t width = 0;
    std::uint32_t value = 0;
    if (first <= 0x7f) { width = 1; value = first; }
    else if (first >= 0xc2 && first <= 0xdf) { width = 2; value = first & 0x1f; }
    else if (first >= 0xe0 && first <= 0xef) { width = 3; value = first & 0x0f; }
    else if (first >= 0xf0 && first <= 0xf4) { width = 4; value = first & 0x07; }
    else return false;
    if (index + width > input.size()) return false;
    for (std::size_t offset = 1; offset < width; ++offset) {
      const auto continuation = static_cast<std::uint8_t>(input[index + offset]);
      if ((continuation & 0xc0) != 0x80) return false;
      value = (value << 6) | (continuation & 0x3f);
    }
    if ((width == 3 && value < 0x800) || (width == 4 && value < 0x10000) ||
        (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff)
      return false;
    index += width;
  }
  return true;
}

struct Api {
#ifdef _WIN32
  HMODULE library = LoadLibraryW(L"winsqlite3.dll");
#else
  void* library = nullptr;
#endif
  using Open = int(ROCKET_SQLITE_CALL*)(const char*, sqlite3**, int, const char*);
  using Close = int(ROCKET_SQLITE_CALL*)(sqlite3*);
  using ErrorMessage = const char*(ROCKET_SQLITE_CALL*)(sqlite3*);
  using Prepare = int(ROCKET_SQLITE_CALL*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
  using BindCount = int(ROCKET_SQLITE_CALL*)(sqlite3_stmt*);
  using BindText = int(ROCKET_SQLITE_CALL*)(sqlite3_stmt*, int, const char*, int,
                                             void(ROCKET_SQLITE_CALL*)(void*));
  using Step = int(ROCKET_SQLITE_CALL*)(sqlite3_stmt*);
  using ColumnCount = int(ROCKET_SQLITE_CALL*)(sqlite3_stmt*);
  using ColumnText = const unsigned char*(ROCKET_SQLITE_CALL*)(sqlite3_stmt*, int);
  using ColumnBytes = int(ROCKET_SQLITE_CALL*)(sqlite3_stmt*, int);
  using Finalize = int(ROCKET_SQLITE_CALL*)(sqlite3_stmt*);
  using Changes = int(ROCKET_SQLITE_CALL*)(sqlite3*);
  using BusyTimeout = int(ROCKET_SQLITE_CALL*)(sqlite3*, int);
  Open open = nullptr;
  Close close = nullptr;
  ErrorMessage errorMessage = nullptr;
  Prepare prepare = nullptr;
  BindCount bindCount = nullptr;
  BindText bindText = nullptr;
  Step step = nullptr;
  ColumnCount columnCount = nullptr;
  ColumnText columnText = nullptr;
  ColumnBytes columnBytes = nullptr;
  Finalize finalize = nullptr;
  Changes changes = nullptr;
  BusyTimeout busyTimeout = nullptr;

  Api() {
#ifdef _WIN32
    if (!library) return;
#define ROCKET_SQLITE(NAME, MEMBER) \
    MEMBER = reinterpret_cast<decltype(MEMBER)>(GetProcAddress(library, NAME))
#else
    for (const char* name : {"libsqlite3.so.0", "libsqlite3.so",
                             "libsqlite3.dylib"}) {
      library = dlopen(name, RTLD_NOW | RTLD_LOCAL);
      if (library) break;
    }
    if (!library) return;
#define ROCKET_SQLITE(NAME, MEMBER) \
    MEMBER = reinterpret_cast<decltype(MEMBER)>(dlsym(library, NAME))
#endif
    ROCKET_SQLITE("sqlite3_open_v2", open);
    ROCKET_SQLITE("sqlite3_close", close);
    ROCKET_SQLITE("sqlite3_errmsg", errorMessage);
    ROCKET_SQLITE("sqlite3_prepare_v2", prepare);
    ROCKET_SQLITE("sqlite3_bind_parameter_count", bindCount);
    ROCKET_SQLITE("sqlite3_bind_text", bindText);
    ROCKET_SQLITE("sqlite3_step", step);
    ROCKET_SQLITE("sqlite3_column_count", columnCount);
    ROCKET_SQLITE("sqlite3_column_text", columnText);
    ROCKET_SQLITE("sqlite3_column_bytes", columnBytes);
    ROCKET_SQLITE("sqlite3_finalize", finalize);
    ROCKET_SQLITE("sqlite3_changes", changes);
    ROCKET_SQLITE("sqlite3_busy_timeout", busyTimeout);
#undef ROCKET_SQLITE
  }

  ~Api() {
#ifdef _WIN32
    if (library) FreeLibrary(library);
#else
    if (library) dlclose(library);
#endif
  }

  bool complete() const {
    return library && open && close && errorMessage && prepare && bindCount &&
           bindText && step && columnCount && columnText && columnBytes &&
           finalize && changes && busyTimeout;
  }
};

inline Api& api() {
  static Api value;
  return value;
}

inline std::string databaseError(sqlite3* database, std::string_view fallback) {
  if (database && api().errorMessage) {
    const char* message = api().errorMessage(database);
    if (message && *message) return message;
  }
  return std::string(fallback);
}

inline bool open(std::string_view path, sqlite3*& database, std::string& error) {
  database = nullptr;
  if (path.empty() || path.size() > 32768 || path.find('\0') != std::string_view::npos) {
    error = "SQLite path must contain 1 through 32768 bytes";
    return false;
  }
  if (!api().complete()) {
    error = "SQLite service is unavailable";
    return false;
  }
  constexpr int openReadWrite = 0x00000002;
  constexpr int openCreate = 0x00000004;
  constexpr int openFullMutex = 0x00010000;
  const std::string pathText(path);
  const int status = api().open(pathText.c_str(), &database,
                                openReadWrite | openCreate | openFullMutex, nullptr);
  if (status != 0) {
    error = databaseError(database, "could not open SQLite database");
    if (database) { api().close(database); database = nullptr; }
    return false;
  }
  if (api().busyTimeout(database, 5000) != 0) {
    error = databaseError(database, "could not configure SQLite busy timeout");
    api().close(database);
    database = nullptr;
    return false;
  }
  return true;
}

inline bool close(sqlite3* database, std::string& error) {
  if (!api().complete()) { error = "SQLite service is unavailable"; return false; }
  if (api().close(database) != 0) {
    error = databaseError(database, "could not close SQLite database");
    return false;
  }
  return true;
}

inline bool prepare(sqlite3* database, std::string_view sql,
                    const std::vector<std::string>& parameters,
                    sqlite3_stmt*& statement, std::string& error) {
  statement = nullptr;
  if (sql.empty() || sql.size() > 1024 * 1024 || parameters.size() > 1024 ||
      sql.find('\0') != std::string_view::npos) {
    error = "SQLite SQL or parameter count exceeds its documented limit";
    return false;
  }
  const std::string sqlText(sql);
  const char* tail = nullptr;
  if (api().prepare(database, sqlText.c_str(), static_cast<int>(sqlText.size()),
                    &statement, &tail) != 0 || !statement) {
    error = databaseError(database, "could not prepare SQLite statement");
    return false;
  }
  while (tail && *tail && (*tail == ' ' || *tail == '\t' || *tail == '\r' || *tail == '\n'))
    ++tail;
  if (tail && *tail) {
    api().finalize(statement);
    statement = nullptr;
    error = "SQLite API accepts exactly one statement per call";
    return false;
  }
  if (api().bindCount(statement) != static_cast<int>(parameters.size())) {
    api().finalize(statement);
    statement = nullptr;
    error = "SQLite parameter count does not match the statement";
    return false;
  }
  auto transient = reinterpret_cast<void(ROCKET_SQLITE_CALL*)(void*)>(
      static_cast<std::intptr_t>(-1));
  for (std::size_t index = 0; index < parameters.size(); ++index) {
    if (parameters[index].size() > 16 * 1024 * 1024 ||
        api().bindText(statement, static_cast<int>(index + 1),
                       parameters[index].data(),
                       static_cast<int>(parameters[index].size()), transient) != 0) {
      error = databaseError(database, "could not bind SQLite text parameter");
      api().finalize(statement);
      statement = nullptr;
      return false;
    }
  }
  return true;
}

inline bool execute(sqlite3* database, std::string_view sql,
                    const std::vector<std::string>& parameters,
                    std::int64_t& changes, std::string& error) {
  sqlite3_stmt* statement = nullptr;
  if (!prepare(database, sql, parameters, statement, error)) return false;
  constexpr int row = 100;
  constexpr int done = 101;
  int status = 0;
  do { status = api().step(statement); } while (status == row);
  if (status != done) error = databaseError(database, "SQLite statement failed");
  const int finalized = api().finalize(statement);
  if (status != done || finalized != 0) {
    if (error.empty()) error = databaseError(database, "could not finalize SQLite statement");
    return false;
  }
  changes = api().changes(database);
  return true;
}

inline bool query(sqlite3* database, std::string_view sql,
                  const std::vector<std::string>& parameters,
                  std::vector<std::vector<std::string>>& rows,
                  std::string& error) {
  sqlite3_stmt* statement = nullptr;
  if (!prepare(database, sql, parameters, statement, error)) return false;
  constexpr int row = 100;
  constexpr int done = 101;
  const int columns = api().columnCount(statement);
  if (columns < 0 || columns > 256) {
    api().finalize(statement);
    error = "SQLite query returns more than 256 columns";
    return false;
  }
  std::uint64_t totalBytes = 0;
  int status = 0;
  while ((status = api().step(statement)) == row) {
    std::vector<std::string> values;
    values.reserve(static_cast<std::size_t>(columns));
    for (int column = 0; column < columns; ++column) {
      const int length = api().columnBytes(statement, column);
      const unsigned char* text = api().columnText(statement, column);
      if (length < 0 || totalBytes + static_cast<std::uint64_t>(length) > 64 * 1024 * 1024) {
        api().finalize(statement);
        error = "SQLite query text exceeds the 64 MiB result limit";
        return false;
      }
      std::string value(text ? reinterpret_cast<const char*>(text) : "",
                        static_cast<std::size_t>(length));
      if (!validUtf8(value)) {
        api().finalize(statement);
        error = "SQLite query returned text that is not valid UTF-8";
        return false;
      }
      values.push_back(std::move(value));
      totalBytes += static_cast<std::uint64_t>(length);
    }
    rows.push_back(std::move(values));
    if (rows.size() > 100000) {
      api().finalize(statement);
      error = "SQLite query returns more than 100000 rows";
      return false;
    }
  }
  if (status != done) error = databaseError(database, "SQLite query failed");
  const int finalized = api().finalize(statement);
  if (status != done || finalized != 0) {
    if (error.empty()) error = databaseError(database, "could not finalize SQLite query");
    return false;
  }
  return true;
}

} // namespace rocket::platform_sqlite

#undef ROCKET_SQLITE_CALL
