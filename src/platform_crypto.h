#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <wintrust.h>
#endif

namespace rocket::platform_crypto {

#ifdef _WIN32
namespace detail {

struct BCryptApi {
  HMODULE library = LoadLibraryW(L"bcrypt.dll");
  decltype(&BCryptGenRandom) genRandom = nullptr;
  decltype(&BCryptOpenAlgorithmProvider) openAlgorithm = nullptr;
  decltype(&BCryptGetProperty) getProperty = nullptr;
  decltype(&BCryptCreateHash) createHash = nullptr;
  decltype(&BCryptHashData) hashData = nullptr;
  decltype(&BCryptFinishHash) finishHash = nullptr;
  decltype(&BCryptDestroyHash) destroyHash = nullptr;
  decltype(&BCryptCloseAlgorithmProvider) closeAlgorithm = nullptr;

  BCryptApi() {
    if (!library) return;
    genRandom = reinterpret_cast<decltype(genRandom)>(GetProcAddress(library, "BCryptGenRandom"));
    openAlgorithm = reinterpret_cast<decltype(openAlgorithm)>(GetProcAddress(library, "BCryptOpenAlgorithmProvider"));
    getProperty = reinterpret_cast<decltype(getProperty)>(GetProcAddress(library, "BCryptGetProperty"));
    createHash = reinterpret_cast<decltype(createHash)>(GetProcAddress(library, "BCryptCreateHash"));
    hashData = reinterpret_cast<decltype(hashData)>(GetProcAddress(library, "BCryptHashData"));
    finishHash = reinterpret_cast<decltype(finishHash)>(GetProcAddress(library, "BCryptFinishHash"));
    destroyHash = reinterpret_cast<decltype(destroyHash)>(GetProcAddress(library, "BCryptDestroyHash"));
    closeAlgorithm = reinterpret_cast<decltype(closeAlgorithm)>(GetProcAddress(library, "BCryptCloseAlgorithmProvider"));
  }

  ~BCryptApi() { if (library) FreeLibrary(library); }

  bool complete() const {
    return library && genRandom && openAlgorithm && getProperty && createHash &&
           hashData && finishHash && destroyHash && closeAlgorithm;
  }
};

inline std::string hex(const std::vector<std::uint8_t>& bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result(bytes.size() * 2, '0');
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    result[index * 2] = digits[bytes[index] >> 4];
    result[index * 2 + 1] = digits[bytes[index] & 0x0f];
  }
  return result;
}

inline bool digestBytes(std::string_view data, std::string_view key,
                        std::vector<std::uint8_t>& output,
                        std::string& error) {
  BCryptApi api;
  if (!api.complete()) {
    error = "Windows CNG cryptography is unavailable";
    return false;
  }
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  const ULONG flags = key.empty() ? 0 : BCRYPT_ALG_HANDLE_HMAC_FLAG;
  if (api.openAlgorithm(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, flags) < 0) {
    error = "could not open the Windows SHA-256 provider";
    return false;
  }
  DWORD objectLength = 0;
  DWORD hashLength = 0;
  DWORD copied = 0;
  bool success = api.getProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                 reinterpret_cast<PUCHAR>(&objectLength),
                                 sizeof(objectLength), &copied, 0) >= 0 &&
                 api.getProperty(algorithm, BCRYPT_HASH_LENGTH,
                                 reinterpret_cast<PUCHAR>(&hashLength),
                                 sizeof(hashLength), &copied, 0) >= 0;
  std::vector<std::uint8_t> object(objectLength);
  output.assign(hashLength, 0);
  if (success)
    success = api.createHash(
        algorithm, &hash, object.data(), static_cast<ULONG>(object.size()),
        key.empty() ? nullptr : reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
        static_cast<ULONG>(key.size()), 0) >= 0;
  if (success && !data.empty())
    success = api.hashData(hash,
                           reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                           static_cast<ULONG>(data.size()), 0) >= 0;
  if (success)
    success = api.finishHash(hash, output.data(),
                             static_cast<ULONG>(output.size()), 0) >= 0;
  if (hash) api.destroyHash(hash);
  api.closeAlgorithm(algorithm, 0);
  if (!success) {
    error = "Windows SHA-256 operation failed";
    return false;
  }
  return true;
}

inline bool digest(std::string_view data, std::string_view key,
                   std::string& result, std::string& error) {
  std::vector<std::uint8_t> output;
  if (!digestBytes(data, key, output, error)) return false;
  result = hex(output);
  return true;
}

inline bool unhex(std::string_view text, std::vector<std::uint8_t>& bytes) {
  if (text.empty() || text.size() % 2 != 0) return false;
  auto value = [](char character) -> int {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
  };
  bytes.resize(text.size() / 2);
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const int high = value(text[index * 2]);
    const int low = value(text[index * 2 + 1]);
    if (high < 0 || low < 0) { bytes.clear(); return false; }
    bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

} // namespace detail
#endif

inline bool secureRandom(std::size_t length, std::vector<std::uint8_t>& result,
                         std::string& error) {
  if (length > 64 * 1024 * 1024) {
    error = "secure random request exceeds the 64 MiB limit";
    return false;
  }
#ifdef _WIN32
  detail::BCryptApi api;
  if (!api.genRandom) {
    error = "Windows CNG secure randomness is unavailable";
    return false;
  }
  result.resize(length);
  if (!result.empty() &&
      api.genRandom(nullptr, result.data(), static_cast<ULONG>(result.size()),
                    BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
    result.clear();
    error = "Windows CNG secure randomness failed";
    return false;
  }
  return true;
#else
  (void)result;
  error = "secure randomness is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool secureInt(std::int64_t minimum, std::int64_t maximum,
                      std::int64_t& result, std::string& error) {
  if (minimum > maximum) {
    error = "secure random minimum exceeds maximum";
    return false;
  }
  const std::uint64_t span = static_cast<std::uint64_t>(maximum) -
                             static_cast<std::uint64_t>(minimum) + 1;
  const std::uint64_t threshold = span == 0 ? 0 : (0 - span) % span;
  while (true) {
    std::vector<std::uint8_t> random;
    if (!secureRandom(sizeof(std::uint64_t), random, error)) return false;
    std::uint64_t sample = 0;
    std::memcpy(&sample, random.data(), sizeof(sample));
    if (sample < threshold) continue;
    const std::uint64_t bits = static_cast<std::uint64_t>(minimum) +
                               (span == 0 ? sample : sample % span);
    std::memcpy(&result, &bits, sizeof(result));
    return true;
  }
}

inline bool sha256(std::string_view data, std::string& result, std::string& error) {
  if (data.size() > 64 * 1024 * 1024) {
    error = "SHA-256 input exceeds the 64 MiB limit";
    return false;
  }
#ifdef _WIN32
  return detail::digest(data, {}, result, error);
#else
  (void)result;
  error = "SHA-256 is currently supported on Windows x64 only";
  return false;
#endif
}

struct SigningKeyPair {
  std::string publicKey;
  std::string privateKey;
};

inline bool generateSigningKeyPair(SigningKeyPair& result,
                                   std::string& error) {
#ifdef _WIN32
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_KEY_HANDLE key = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM,
                                  nullptr, 0) < 0 ||
      BCryptGenerateKeyPair(algorithm, &key, 256, 0) < 0 ||
      BCryptFinalizeKeyPair(key, 0) < 0) {
    if (key) BCryptDestroyKey(key);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    error = "could not generate an ECDSA P-256 registry signing key";
    return false;
  }
  auto exported = [&](LPCWSTR type, std::string& encoded) -> bool {
    ULONG size = 0;
    if (BCryptExportKey(key, nullptr, type, nullptr, 0, &size, 0) < 0)
      return false;
    std::vector<std::uint8_t> bytes(size);
    if (BCryptExportKey(key, nullptr, type, bytes.data(), size, &size, 0) < 0)
      return false;
    bytes.resize(size);
    encoded = detail::hex(bytes);
    return true;
  };
  const bool success = exported(BCRYPT_ECCPUBLIC_BLOB, result.publicKey) &&
                       exported(BCRYPT_ECCPRIVATE_BLOB, result.privateKey);
  BCryptDestroyKey(key);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (!success) {
    error = "could not export an ECDSA P-256 registry signing key";
    return false;
  }
  return true;
#else
  (void)result;
  error = "registry signing is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool signingKeyFingerprint(std::string_view publicKey,
                                  std::string& fingerprint,
                                  std::string& error) {
#ifdef _WIN32
  std::vector<std::uint8_t> bytes;
  if (!detail::unhex(publicKey, bytes)) {
    error = "registry public signing key is not valid hexadecimal data";
    return false;
  }
  std::vector<std::uint8_t> digest;
  if (!detail::digestBytes(
          std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
          {}, digest, error))
    return false;
  fingerprint = "sha256:" + detail::hex(digest);
  return true;
#else
  (void)publicKey; (void)fingerprint;
  error = "registry signing is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool sign(std::string_view privateKey, std::string_view data,
                 std::string& signature, std::string& error) {
#ifdef _WIN32
  std::vector<std::uint8_t> keyBytes;
  if (!detail::unhex(privateKey, keyBytes)) {
    error = "registry private signing key is invalid";
    return false;
  }
  std::vector<std::uint8_t> digest;
  if (!detail::digestBytes(data, {}, digest, error)) return false;
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_KEY_HANDLE key = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM,
                                  nullptr, 0) < 0 ||
      BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key,
                          keyBytes.data(), static_cast<ULONG>(keyBytes.size()),
                          0) < 0) {
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    error = "could not import the registry private signing key";
    return false;
  }
  ULONG size = 0;
  bool success = BCryptSignHash(key, nullptr, digest.data(),
                                static_cast<ULONG>(digest.size()), nullptr, 0,
                                &size, 0) >= 0;
  std::vector<std::uint8_t> bytes(size);
  if (success)
    success = BCryptSignHash(key, nullptr, digest.data(),
                             static_cast<ULONG>(digest.size()), bytes.data(),
                             size, &size, 0) >= 0;
  BCryptDestroyKey(key);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (!success) {
    error = "could not sign registry metadata";
    return false;
  }
  bytes.resize(size);
  signature = detail::hex(bytes);
  return true;
#else
  (void)privateKey; (void)data; (void)signature;
  error = "registry signing is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool verifySignature(std::string_view publicKey, std::string_view data,
                            std::string_view signature, bool& trusted,
                            std::string& error) {
  trusted = false;
#ifdef _WIN32
  std::vector<std::uint8_t> keyBytes;
  std::vector<std::uint8_t> signatureBytes;
  if (!detail::unhex(publicKey, keyBytes) ||
      !detail::unhex(signature, signatureBytes)) {
    error = "registry signature or public key is invalid";
    return false;
  }
  std::vector<std::uint8_t> digest;
  if (!detail::digestBytes(data, {}, digest, error)) return false;
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_KEY_HANDLE key = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM,
                                  nullptr, 0) < 0 ||
      BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key,
                          keyBytes.data(), static_cast<ULONG>(keyBytes.size()),
                          0) < 0) {
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    error = "could not import the registry public signing key";
    return false;
  }
  trusted = BCryptVerifySignature(
                key, nullptr, digest.data(), static_cast<ULONG>(digest.size()),
                signatureBytes.data(), static_cast<ULONG>(signatureBytes.size()),
                0) >= 0;
  BCryptDestroyKey(key);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  return true;
#else
  (void)publicKey; (void)data; (void)signature;
  error = "registry signing is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool hmacSha256(std::string_view key, std::string_view data,
                       std::string& result, std::string& error) {
  if (key.empty()) {
    error = "HMAC-SHA-256 keys must not be empty";
    return false;
  }
  if (key.size() > 1024 * 1024 || data.size() > 64 * 1024 * 1024) {
    error = "HMAC-SHA-256 input exceeds its documented limit";
    return false;
  }
#ifdef _WIN32
  return detail::digest(data, key, result, error);
#else
  (void)result;
  error = "HMAC-SHA-256 is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool constantTimeEqual(std::string_view left, std::string_view right) {
  const std::size_t length = left.size() > right.size() ? left.size() : right.size();
  std::uint64_t difference = static_cast<std::uint64_t>(left.size() ^ right.size());
  for (std::size_t index = 0; index < length; ++index) {
    const std::uint8_t a = index < left.size() ? static_cast<std::uint8_t>(left[index]) : 0;
    const std::uint8_t b = index < right.size() ? static_cast<std::uint8_t>(right[index]) : 0;
    difference |= static_cast<std::uint64_t>(a ^ b);
  }
  return difference == 0;
}

inline bool verifySignedFile(const std::wstring& path, bool& trusted,
                             std::string& error) {
#ifdef _WIN32
  if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    error = "signed-file path does not exist";
    return false;
  }
  HMODULE library = LoadLibraryW(L"wintrust.dll");
  if (!library) {
    error = "Windows certificate verification is unavailable";
    return false;
  }
  using Verify = LONG(WINAPI*)(HWND, GUID*, LPVOID);
  const auto verify = reinterpret_cast<Verify>(GetProcAddress(library, "WinVerifyTrust"));
  if (!verify) {
    FreeLibrary(library);
    error = "Windows certificate verification is unavailable";
    return false;
  }
  WINTRUST_FILE_INFO file{};
  file.cbStruct = sizeof(file);
  file.pcwszFilePath = path.c_str();
  WINTRUST_DATA data{};
  data.cbStruct = sizeof(data);
  data.dwUIChoice = WTD_UI_NONE;
  data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
  data.dwUnionChoice = WTD_CHOICE_FILE;
  data.pFile = &file;
  data.dwStateAction = WTD_STATEACTION_VERIFY;
  data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL |
                     WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  GUID action = {0x00aac56b, 0xcd44, 0x11d0,
                 {0x8c, 0xc2, 0x00, 0xc0, 0x4f, 0xc2, 0x95, 0xee}};
  const LONG status = verify(nullptr, &action, &data);
  trusted = status == ERROR_SUCCESS;
  data.dwStateAction = WTD_STATEACTION_CLOSE;
  verify(nullptr, &action, &data);
  FreeLibrary(library);
  return true;
#else
  (void)path;
  (void)trusted;
  error = "certificate verification is currently supported on Windows x64 only";
  return false;
#endif
}

} // namespace rocket::platform_crypto
