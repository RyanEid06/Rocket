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
#include <compressapi.h>
#endif

namespace rocket::platform_compression {

inline bool compressXpress(std::string_view input, std::string& output,
                           std::string& error) {
  if (input.size() > 64 * 1024 * 1024) {
    error = "compression input exceeds the 64 MiB limit";
    return false;
  }
#ifdef _WIN32
  HMODULE library = LoadLibraryW(L"cabinet.dll");
  if (!library) { error = "Windows Compression API is unavailable"; return false; }
  using Create = BOOL(WINAPI*)(DWORD, PCOMPRESS_ALLOCATION_ROUTINES, COMPRESSOR_HANDLE*);
  using Compress = BOOL(WINAPI*)(COMPRESSOR_HANDLE, PVOID, SIZE_T, PVOID, SIZE_T, PSIZE_T);
  using Close = BOOL(WINAPI*)(COMPRESSOR_HANDLE);
  const auto create = reinterpret_cast<Create>(GetProcAddress(library, "CreateCompressor"));
  const auto compress = reinterpret_cast<Compress>(GetProcAddress(library, "Compress"));
  const auto close = reinterpret_cast<Close>(GetProcAddress(library, "CloseCompressor"));
  if (!create || !compress || !close) {
    FreeLibrary(library);
    error = "Windows Compression API is unavailable";
    return false;
  }
  COMPRESSOR_HANDLE handle = nullptr;
  if (!create(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &handle)) {
    error = "could not create an XPRESS Huffman compressor";
    FreeLibrary(library);
    return false;
  }
  SIZE_T required = 0;
  const BOOL queried = compress(handle,
      input.empty() ? nullptr : const_cast<char*>(input.data()), input.size(),
      nullptr, 0, &required);
  if (!queried && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    close(handle); FreeLibrary(library);
    error = "could not size the XPRESS Huffman output";
    return false;
  }
  if (required > 64 * 1024 * 1024 + 1024) {
    close(handle); FreeLibrary(library);
    error = "compressed output exceeds its documented limit";
    return false;
  }
  output.resize(required);
  SIZE_T written = 0;
  const BOOL succeeded = compress(handle,
      input.empty() ? nullptr : const_cast<char*>(input.data()), input.size(),
      output.empty() ? nullptr : output.data(), output.size(), &written);
  close(handle); FreeLibrary(library);
  if (!succeeded) {
    output.clear();
    error = "XPRESS Huffman compression failed";
    return false;
  }
  output.resize(written);
  return true;
#else
  (void)output;
  error = "compression is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool decompressXpress(std::string_view input, std::string& output,
                             std::string& error) {
  if (input.size() > 64 * 1024 * 1024 + 1024) {
    error = "compressed input exceeds its documented limit";
    return false;
  }
#ifdef _WIN32
  HMODULE library = LoadLibraryW(L"cabinet.dll");
  if (!library) { error = "Windows Compression API is unavailable"; return false; }
  using Create = BOOL(WINAPI*)(DWORD, PCOMPRESS_ALLOCATION_ROUTINES, DECOMPRESSOR_HANDLE*);
  using Decompress = BOOL(WINAPI*)(DECOMPRESSOR_HANDLE, PVOID, SIZE_T, PVOID, SIZE_T, PSIZE_T);
  using Close = BOOL(WINAPI*)(DECOMPRESSOR_HANDLE);
  const auto create = reinterpret_cast<Create>(GetProcAddress(library, "CreateDecompressor"));
  const auto decompress = reinterpret_cast<Decompress>(GetProcAddress(library, "Decompress"));
  const auto close = reinterpret_cast<Close>(GetProcAddress(library, "CloseDecompressor"));
  if (!create || !decompress || !close) {
    FreeLibrary(library);
    error = "Windows Compression API is unavailable";
    return false;
  }
  DECOMPRESSOR_HANDLE handle = nullptr;
  if (!create(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &handle)) {
    error = "could not create an XPRESS Huffman decompressor";
    FreeLibrary(library);
    return false;
  }
  SIZE_T required = 0;
  const BOOL queried = decompress(handle,
      input.empty() ? nullptr : const_cast<char*>(input.data()), input.size(),
      nullptr, 0, &required);
  if (!queried && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    close(handle); FreeLibrary(library);
    error = "compressed data is invalid or truncated";
    return false;
  }
  if (required > 64 * 1024 * 1024) {
    close(handle); FreeLibrary(library);
    error = "decompressed output exceeds the 64 MiB limit";
    return false;
  }
  output.resize(required);
  SIZE_T written = 0;
  const BOOL succeeded = decompress(handle,
      input.empty() ? nullptr : const_cast<char*>(input.data()), input.size(),
      output.empty() ? nullptr : output.data(), output.size(), &written);
  close(handle); FreeLibrary(library);
  if (!succeeded) {
    output.clear();
    error = "compressed data is invalid or truncated";
    return false;
  }
  output.resize(written);
  return true;
#else
  (void)output;
  error = "compression is currently supported on Windows x64 only";
  return false;
#endif
}

} // namespace rocket::platform_compression
