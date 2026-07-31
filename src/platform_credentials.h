#pragma once

#include "platform_crypto.h"

#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincred.h>
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

inline bool target(std::string_view registry, std::wstring& result,
                   std::string& error) {
  if (registry.empty()) { error = "registry identity must not be empty"; return false; }
  std::string digest;
  if (!platform_crypto::sha256(registry, digest, error)) return false;
#ifdef _WIN32
  result = L"Rocket/Registry/" + wide(digest);
  return true;
#else
  (void)result;
  error = "credential storage is currently supported on Windows x64 only";
  return false;
#endif
}

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
  (void)registry; (void)token;
  error = "credential storage is currently supported on Windows x64 only";
  return false;
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
  (void)registry; (void)token;
  error = "credential storage is currently supported on Windows x64 only";
  return false;
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
  (void)registry;
  error = "credential storage is currently supported on Windows x64 only";
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
  (void)secret;
  error = "credential input is currently supported on Windows x64 only";
  return false;
#endif
}

} // namespace rocket::platform_credentials
