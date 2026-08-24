#pragma once

#include "platform_crypto.h"

#include <iostream>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincred.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace rocket::platform_credentials {

#ifdef _WIN32
inline std::wstring wide(std::string_view value) {
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          value.data(),
                                          static_cast<int>(value.size()),
                                          nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(),
                          length) <= 0)
    return {};
  return result;
}
#endif

#ifndef _WIN32
inline bool credentialPath(std::string_view registry,
                           std::filesystem::path& result,
                           std::string& error) {
  if (registry.empty()) {
    error = "registry identity must not be empty";
    return false;
  }
  std::string digest;
  if (!platform_crypto::sha256(registry, digest, error)) return false;
  const char* configured = std::getenv("XDG_CONFIG_HOME");
  std::filesystem::path root;
  if (configured && *configured) {
    root = std::filesystem::u8path(configured);
  } else {
    const char* userHome = std::getenv("HOME");
    if (!userHome || !*userHome) {
      error = "credential storage requires XDG_CONFIG_HOME or HOME";
      return false;
    }
    root = std::filesystem::u8path(userHome) / ".config";
  }
  result = root / "rocket" / "credentials" / (digest + ".token");
  return true;
}
#endif

#ifdef _WIN32
inline bool target(std::string_view registry, std::wstring& result,
                   std::string& error) {
  if (registry.empty()) { error = "registry identity must not be empty"; return false; }
  std::string digest;
  if (!platform_crypto::sha256(registry, digest, error)) return false;
  result = L"Rocket/Registry/" + wide(digest);
  return true;
}
#endif

inline bool store(std::string_view registry, std::string_view token,
                  std::string& error) {
#ifdef _WIN32
  if (token.empty() || token.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
    error = "registry token is empty or exceeds the credential-store limit";
    return false;
  }
  std::wstring targetName;
  if (!target(registry, targetName, error)) return false;
  std::wstring user = L"rocket-registry-token";
  CREDENTIALW credential{};
  credential.Type = CRED_TYPE_GENERIC;
  credential.TargetName = targetName.data();
  credential.CredentialBlobSize = static_cast<DWORD>(token.size());
  credential.CredentialBlob = reinterpret_cast<LPBYTE>(
      const_cast<char*>(token.data()));
  credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
  credential.UserName = user.data();
  if (!CredWriteW(&credential, 0)) {
    // Non-interactive Windows build agents can expose Credential Manager while
    // refusing machine-persistent writes. Session persistence retains the same
    // OS-protected, non-file credential boundary for that environment.
    credential.Persist = CRED_PERSIST_SESSION;
    if (!CredWriteW(&credential, 0)) {
      error = "Windows Credential Manager refused the registry credential (error " +
              std::to_string(GetLastError()) + ")";
      return false;
    }
  }
  return true;
#else
  if (token.empty() || token.size() > 1024 * 1024) {
    error = "registry token is empty or exceeds the credential-store limit";
    return false;
  }
  std::filesystem::path path;
  if (!credentialPath(registry, path, error)) return false;
  std::error_code directoryError;
  std::filesystem::create_directories(path.parent_path(), directoryError);
  if (directoryError || ::chmod(path.parent_path().c_str(), 0700) != 0) {
    error = "could not create the private credential directory";
    return false;
  }
  static std::atomic<std::uint64_t> sequence{0};
  const std::filesystem::path temporary =
      path.string() + ".tmp." + std::to_string(static_cast<long long>(::getpid())) +
      "." + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
  const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL,
                                S_IRUSR | S_IWUSR);
  if (descriptor < 0) {
    error = "could not create a private temporary credential file: " +
            std::string(std::strerror(errno));
    return false;
  }
  std::size_t offset = 0;
  while (offset < token.size()) {
    const ssize_t written = ::write(descriptor, token.data() + offset,
                                    token.size() - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) break;
    offset += static_cast<std::size_t>(written);
  }
  const bool synced = offset == token.size() && ::fsync(descriptor) == 0;
  const int closed = ::close(descriptor);
  if (!synced || closed != 0 || ::rename(temporary.c_str(), path.c_str()) != 0 ||
      ::chmod(path.c_str(), S_IRUSR | S_IWUSR) != 0) {
    const int saved = errno;
    ::unlink(temporary.c_str());
    error = "could not commit the private credential file: " +
            std::string(std::strerror(saved));
    return false;
  }
  return true;
#endif
}

inline bool load(std::string_view registry, std::string& token,
                 std::string& error) {
#ifdef _WIN32
  std::wstring targetName;
  if (!target(registry, targetName, error)) return false;
  PCREDENTIALW credential = nullptr;
  if (!CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
    error = "no stored credential exists for this registry";
    return false;
  }
  token.assign(reinterpret_cast<const char*>(credential->CredentialBlob),
               credential->CredentialBlobSize);
  CredFree(credential);
  return true;
#else
  std::filesystem::path path;
  if (!credentialPath(registry, path, error)) return false;
  struct stat status {};
  if (::lstat(path.c_str(), &status) != 0 || !S_ISREG(status.st_mode)) {
    error = "no stored credential exists for this registry";
    return false;
  }
  if (status.st_uid != ::geteuid() || (status.st_mode & (S_IRWXG | S_IRWXO)) != 0 ||
      status.st_size < 1 || status.st_size > 1024 * 1024) {
    error = "stored registry credential has unsafe ownership or permissions";
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  token.assign(std::istreambuf_iterator<char>(input),
               std::istreambuf_iterator<char>());
  if (!input.eof() || token.empty()) {
    token.clear();
    error = "could not read the stored registry credential";
    return false;
  }
  return true;
#endif
}

inline bool erase(std::string_view registry, std::string& error) {
#ifdef _WIN32
  std::wstring targetName;
  if (!target(registry, targetName, error)) return false;
  if (!CredDeleteW(targetName.c_str(), CRED_TYPE_GENERIC, 0)) {
    if (GetLastError() == ERROR_NOT_FOUND) return true;
    error = "Windows Credential Manager could not delete the registry credential";
    return false;
  }
  return true;
#else
  std::filesystem::path path;
  if (!credentialPath(registry, path, error)) return false;
  if (::unlink(path.c_str()) == 0 || errno == ENOENT) return true;
  error = "could not delete the stored registry credential: " +
          std::string(std::strerror(errno));
  return false;
#endif
}

inline bool readSecretLine(std::string& secret, std::string& error) {
#ifdef _WIN32
  HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  const bool console = input != INVALID_HANDLE_VALUE &&
                       GetConsoleMode(input, &mode) != 0;
  if (console) SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT);
  const bool read = static_cast<bool>(std::getline(std::cin, secret));
  if (console) {
    SetConsoleMode(input, mode);
    std::cerr << '\n';
  }
  if (!read || secret.empty()) {
    secret.clear();
    error = "could not read a non-empty registry token from standard input";
    return false;
  }
  if (!secret.empty() && secret.back() == '\r') secret.pop_back();
  return true;
#else
  termios mode{};
  const bool terminal = ::isatty(STDIN_FILENO) == 1 &&
                        ::tcgetattr(STDIN_FILENO, &mode) == 0;
  termios hidden = mode;
  if (terminal) {
    hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden);
  }
  const bool read = static_cast<bool>(std::getline(std::cin, secret));
  if (terminal) {
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &mode);
    std::cerr << '\n';
  }
  if (!read || secret.empty()) {
    secret.clear();
    error = "could not read a non-empty registry token from standard input";
    return false;
  }
  if (!secret.empty() && secret.back() == '\r') secret.pop_back();
  return true;
#endif
}

} // namespace rocket::platform_credentials
