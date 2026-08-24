#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <wintrust.h>
#else
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/hmac.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif
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

#ifndef _WIN32
namespace detail {

inline std::string hex(const std::vector<std::uint8_t>& bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result(bytes.size() * 2, '0');
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    result[index * 2] = digits[bytes[index] >> 4];
    result[index * 2 + 1] = digits[bytes[index] & 0x0f];
  }
  return result;
}

inline bool unhex(std::string_view text, std::vector<std::uint8_t>& bytes) {
  if (text.empty() || text.size() % 2 != 0) return false;
  const auto value = [](char character) -> int {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
  };
  bytes.resize(text.size() / 2);
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const int high = value(text[index * 2]);
    const int low = value(text[index * 2 + 1]);
    if (high < 0 || low < 0) {
      bytes.clear();
      return false;
    }
    bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}

inline bool digestBytes(std::string_view data, std::string_view key,
                        std::vector<std::uint8_t>& output,
                        std::string& error) {
  output.resize(SHA256_DIGEST_LENGTH);
  if (key.empty()) {
    if (!SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                output.data())) {
      error = "OpenSSL SHA-256 operation failed";
      return false;
    }
    return true;
  }
  unsigned int length = 0;
  if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
            reinterpret_cast<const unsigned char*>(data.data()), data.size(),
            output.data(), &length) || length != SHA256_DIGEST_LENGTH) {
    error = "OpenSSL HMAC-SHA-256 operation failed";
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

inline std::uint32_t readLittleU32(const std::uint8_t* bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

inline void appendLittleU32(std::vector<std::uint8_t>& bytes,
                            std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value));
  bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
  bytes.push_back(static_cast<std::uint8_t>(value >> 16U));
  bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
}

inline EC_KEY* importP256(std::string_view encoded, bool requirePrivate,
                          std::string& error) {
  std::vector<std::uint8_t> bytes;
  if (!unhex(encoded, bytes) ||
      bytes.size() != (requirePrivate ? 104U : 72U) ||
      readLittleU32(bytes.data() + 4) != 32U ||
      readLittleU32(bytes.data()) !=
          (requirePrivate ? 0x32534345U : 0x31534345U)) {
    error = requirePrivate ? "registry private signing key is invalid"
                           : "registry public signing key is invalid";
    return nullptr;
  }
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  BIGNUM* x = BN_bin2bn(bytes.data() + 8, 32, nullptr);
  BIGNUM* y = BN_bin2bn(bytes.data() + 40, 32, nullptr);
  EC_POINT* point = key ? EC_POINT_new(EC_KEY_get0_group(key)) : nullptr;
  bool valid = key && x && y && point &&
               EC_POINT_set_affine_coordinates(EC_KEY_get0_group(key), point,
                                               x, y, nullptr) == 1 &&
               EC_KEY_set_public_key(key, point) == 1;
  BIGNUM* privateValue = nullptr;
  if (valid && requirePrivate) {
    privateValue = BN_bin2bn(bytes.data() + 72, 32, nullptr);
    valid = privateValue && EC_KEY_set_private_key(key, privateValue) == 1;
  }
  BN_free(privateValue);
  EC_POINT_free(point);
  BN_free(y);
  BN_free(x);
  if (valid && EC_KEY_check_key(key) == 1) return key;
  EC_KEY_free(key);
  error = requirePrivate ? "could not import the registry private signing key"
                         : "could not import the registry public signing key";
  return nullptr;
}

inline bool exportP256(EC_KEY* key, bool includePrivate, std::string& encoded) {
  const EC_GROUP* group = EC_KEY_get0_group(key);
  const EC_POINT* point = EC_KEY_get0_public_key(key);
  BIGNUM* x = BN_new();
  BIGNUM* y = BN_new();
  bool valid = group && point && x && y &&
               EC_POINT_get_affine_coordinates(group, point, x, y, nullptr) == 1;
  const BIGNUM* privateValue = includePrivate ? EC_KEY_get0_private_key(key) : nullptr;
  if (includePrivate && !privateValue) valid = false;
  std::vector<std::uint8_t> bytes;
  appendLittleU32(bytes, includePrivate ? 0x32534345U : 0x31534345U);
  appendLittleU32(bytes, 32U);
  bytes.resize(includePrivate ? 104U : 72U);
  if (valid)
    valid = BN_bn2binpad(x, bytes.data() + 8, 32) == 32 &&
            BN_bn2binpad(y, bytes.data() + 40, 32) == 32 &&
            (!includePrivate ||
             BN_bn2binpad(privateValue, bytes.data() + 72, 32) == 32);
  BN_free(y);
  BN_free(x);
  if (!valid) return false;
  encoded = hex(bytes);
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
  result.resize(length);
  if (!result.empty() &&
      RAND_bytes(result.data(), static_cast<int>(result.size())) != 1) {
    result.clear();
    error = "OpenSSL secure randomness failed";
    return false;
  }
  return true;
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
  return detail::digest(data, {}, result, error);
#endif
}

// Internal artifact hashing is streaming and is intentionally separate from
// the public bounded std.crypto byte-buffer contract above. Compiler and
// runtime binaries can legitimately exceed 64 MiB in Debug configurations.
inline bool sha256File(const std::filesystem::path& path, std::string& result,
                       std::string& error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "could not open file for SHA-256: " + path.string();
    return false;
  }
  std::array<char, 64 * 1024> buffer{};
  std::vector<std::uint8_t> output(32);
#ifdef _WIN32
  detail::BCryptApi api;
  if (!api.complete()) {
    error = "Windows CNG cryptography is unavailable";
    return false;
  }
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD objectLength = 0;
  DWORD hashLength = 0;
  DWORD copied = 0;
  bool success = api.openAlgorithm(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                   nullptr, 0) >= 0 &&
                 api.getProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                 reinterpret_cast<PUCHAR>(&objectLength),
                                 sizeof(objectLength), &copied, 0) >= 0 &&
                 api.getProperty(algorithm, BCRYPT_HASH_LENGTH,
                                 reinterpret_cast<PUCHAR>(&hashLength),
                                 sizeof(hashLength), &copied, 0) >= 0;
  std::vector<std::uint8_t> object(objectLength);
  output.assign(hashLength, 0);
  if (success)
    success = api.createHash(algorithm, &hash, object.data(), objectLength,
                             nullptr, 0, 0) >= 0;
  while (success && input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    if (count > 0)
      success = api.hashData(
                    hash, reinterpret_cast<PUCHAR>(buffer.data()),
                    static_cast<ULONG>(count), 0) >= 0;
  }
  success = success && input.eof();
  if (success)
    success = api.finishHash(hash, output.data(),
                             static_cast<ULONG>(output.size()), 0) >= 0;
  if (hash) api.destroyHash(hash);
  if (algorithm) api.closeAlgorithm(algorithm, 0);
  if (!success) {
    error = "could not stream file through Windows SHA-256 provider";
    return false;
  }
#else
  SHA256_CTX context;
  bool success = SHA256_Init(&context) == 1;
  while (success && input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    if (count > 0)
      success = SHA256_Update(&context, buffer.data(),
                              static_cast<std::size_t>(count)) == 1;
  }
  success = success && input.eof() &&
            SHA256_Final(output.data(), &context) == 1;
  if (!success) {
    error = "could not stream file through OpenSSL SHA-256 provider";
    return false;
  }
#endif
  result = detail::hex(output);
  return true;
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
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (!key || EC_KEY_generate_key(key) != 1 ||
      !detail::exportP256(key, false, result.publicKey) ||
      !detail::exportP256(key, true, result.privateKey)) {
    EC_KEY_free(key);
    error = "could not generate an ECDSA P-256 registry signing key";
    return false;
  }
  EC_KEY_free(key);
  return true;
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
  std::vector<std::uint8_t> bytes;
  if (!detail::unhex(publicKey, bytes)) {
    error = "registry public signing key is not valid hexadecimal data";
    return false;
  }
  std::vector<std::uint8_t> digest;
  if (!detail::digestBytes(
          std::string_view(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size()),
          {}, digest, error))
    return false;
  fingerprint = "sha256:" + detail::hex(digest);
  return true;
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
  EC_KEY* key = detail::importP256(privateKey, true, error);
  if (!key) return false;
  std::vector<std::uint8_t> digest;
  if (!detail::digestBytes(data, {}, digest, error)) {
    EC_KEY_free(key);
    return false;
  }
  ECDSA_SIG* signedDigest = ECDSA_do_sign(
      digest.data(), static_cast<int>(digest.size()), key);
  EC_KEY_free(key);
  if (!signedDigest) {
    error = "could not sign registry metadata";
    return false;
  }
  const BIGNUM* r = nullptr;
  const BIGNUM* s = nullptr;
  ECDSA_SIG_get0(signedDigest, &r, &s);
  std::vector<std::uint8_t> bytes(64);
  const bool encoded = BN_bn2binpad(r, bytes.data(), 32) == 32 &&
                       BN_bn2binpad(s, bytes.data() + 32, 32) == 32;
  ECDSA_SIG_free(signedDigest);
  if (!encoded) {
    error = "could not encode registry metadata signature";
    return false;
  }
  signature = detail::hex(bytes);
  return true;
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
  EC_KEY* key = detail::importP256(publicKey, false, error);
  if (!key) return false;
  std::vector<std::uint8_t> signatureBytes;
  if (!detail::unhex(signature, signatureBytes) || signatureBytes.size() != 64) {
    EC_KEY_free(key);
    error = "registry signature or public key is invalid";
    return false;
  }
  std::vector<std::uint8_t> digest;
  if (!detail::digestBytes(data, {}, digest, error)) {
    EC_KEY_free(key);
    return false;
  }
  ECDSA_SIG* signedDigest = ECDSA_SIG_new();
  BIGNUM* r = BN_bin2bn(signatureBytes.data(), 32, nullptr);
  BIGNUM* s = BN_bin2bn(signatureBytes.data() + 32, 32, nullptr);
  if (!signedDigest || !r || !s || ECDSA_SIG_set0(signedDigest, r, s) != 1) {
    BN_free(r);
    BN_free(s);
    ECDSA_SIG_free(signedDigest);
    EC_KEY_free(key);
    error = "registry signature or public key is invalid";
    return false;
  }
  trusted = ECDSA_do_verify(digest.data(), static_cast<int>(digest.size()),
                            signedDigest, key) == 1;
  ECDSA_SIG_free(signedDigest);
  EC_KEY_free(key);
  return true;
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
  return detail::digest(data, key, result, error);
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

inline bool verifySignedFile(const std::filesystem::path& path, bool& trusted,
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
#elif defined(__APPLE__)
  if (path.empty() || !std::filesystem::is_regular_file(path)) {
    error = "signed-file path does not exist";
    return false;
  }
  const std::string nativePath = path.string();
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(nativePath.data()),
      static_cast<CFIndex>(nativePath.size()), false);
  if (!url) {
    error = "could not create the macOS signed-file URL";
    return false;
  }
  SecStaticCodeRef code = nullptr;
  const OSStatus created = SecStaticCodeCreateWithPath(
      url, kSecCSDefaultFlags, &code);
  CFRelease(url);
  if (created != errSecSuccess || !code) {
    error = "macOS could not inspect the executable signature (status " +
            std::to_string(created) + ")";
    if (code) CFRelease(code);
    return false;
  }
  const OSStatus status = SecStaticCodeCheckValidity(
      code, kSecCSCheckAllArchitectures, nullptr);
  CFRelease(code);
  trusted = status == errSecSuccess;
  return true;
#else
  if (path.empty() || !std::filesystem::is_regular_file(path)) {
    error = "signed-file path does not exist";
    return false;
  }
  // Linux has no universal executable-signature authority. The portable
  // library reports an inspected but untrusted regular file deterministically.
  trusted = false;
  return true;
#endif
}

} // namespace rocket::platform_crypto
