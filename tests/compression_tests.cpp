#include "platform_compression.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <compressapi.h>
#endif

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

void roundTrip(const std::string& input, std::string_view name) {
  std::string compressed;
  std::string restored;
  std::string error;
  expect(rocket::platform_compression::compressXpress(input, compressed, error),
         std::string(name) + " compresses: " + error);
  if (compressed.empty()) return;
  error.clear();
  expect(rocket::platform_compression::decompressXpress(
             compressed, restored, error),
         std::string(name) + " decompresses: " + error);
  expect(restored == input, std::string(name) + " round trips exactly");
}

#ifdef _WIN32
struct CabinetApi {
  using CreateCompressorFn = BOOL(WINAPI*)(
      DWORD, PCOMPRESS_ALLOCATION_ROUTINES, COMPRESSOR_HANDLE*);
  using CreateDecompressorFn = BOOL(WINAPI*)(
      DWORD, PCOMPRESS_ALLOCATION_ROUTINES, DECOMPRESSOR_HANDLE*);
  using CompressFn = BOOL(WINAPI*)(
      COMPRESSOR_HANDLE, PVOID, SIZE_T, PVOID, SIZE_T, PSIZE_T);
  using DecompressFn = BOOL(WINAPI*)(
      DECOMPRESSOR_HANDLE, PVOID, SIZE_T, PVOID, SIZE_T, PSIZE_T);
  using CloseCompressorFn = BOOL(WINAPI*)(COMPRESSOR_HANDLE);
  using CloseDecompressorFn = BOOL(WINAPI*)(DECOMPRESSOR_HANDLE);

  HMODULE module = LoadLibraryW(L"cabinet.dll");
  CreateCompressorFn createCompressor = nullptr;
  CreateDecompressorFn createDecompressor = nullptr;
  CompressFn compress = nullptr;
  DecompressFn decompress = nullptr;
  CloseCompressorFn closeCompressor = nullptr;
  CloseDecompressorFn closeDecompressor = nullptr;

  CabinetApi() {
    if (module == nullptr) return;
    createCompressor = reinterpret_cast<CreateCompressorFn>(
        GetProcAddress(module, "CreateCompressor"));
    createDecompressor = reinterpret_cast<CreateDecompressorFn>(
        GetProcAddress(module, "CreateDecompressor"));
    compress = reinterpret_cast<CompressFn>(GetProcAddress(module, "Compress"));
    decompress = reinterpret_cast<DecompressFn>(
        GetProcAddress(module, "Decompress"));
    closeCompressor = reinterpret_cast<CloseCompressorFn>(
        GetProcAddress(module, "CloseCompressor"));
    closeDecompressor = reinterpret_cast<CloseDecompressorFn>(
        GetProcAddress(module, "CloseDecompressor"));
  }

  ~CabinetApi() {
    if (module != nullptr) FreeLibrary(module);
  }

  bool complete() const {
    return module != nullptr && createCompressor != nullptr &&
           createDecompressor != nullptr && compress != nullptr &&
           decompress != nullptr && closeCompressor != nullptr &&
           closeDecompressor != nullptr;
  }
};

void windowsInteroperability(const std::string& input) {
  CabinetApi api;
  expect(api.complete(), "Windows Compression API is available");
  if (!api.complete()) return;

  std::string portable;
  std::string error;
  expect(rocket::platform_compression::compressXpress(input, portable, error),
         "portable compressor creates an interoperability stream");
  if (portable.empty()) return;

  DECOMPRESSOR_HANDLE decompressor = nullptr;
  expect(api.createDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr,
                                &decompressor) != 0,
         "Windows XPRESS-Huffman decompressor opens");
  if (decompressor == nullptr) return;
  std::string windowsRestored(input.size(), '\0');
  SIZE_T written = 0;
  const BOOL decoded = api.decompress(
      decompressor, portable.data(), portable.size(), windowsRestored.data(),
      windowsRestored.size(), &written);
  api.closeDecompressor(decompressor);
  expect(decoded != 0 && written == input.size() && windowsRestored == input,
         "Windows API decodes the portable XPRESS-Huffman stream");

  COMPRESSOR_HANDLE compressor = nullptr;
  expect(api.createCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr,
                              &compressor) != 0,
         "Windows XPRESS-Huffman compressor opens");
  if (compressor == nullptr) return;
  SIZE_T required = 0;
  api.compress(compressor, const_cast<char*>(input.data()), input.size(),
               nullptr, 0, &required);
  std::string windowsCompressed(required, '\0');
  SIZE_T compressedBytes = 0;
  const BOOL encoded = api.compress(
      compressor, const_cast<char*>(input.data()), input.size(),
      windowsCompressed.data(), windowsCompressed.size(), &compressedBytes);
  api.closeCompressor(compressor);
  windowsCompressed.resize(compressedBytes);
  expect(encoded != 0, "Windows API creates an XPRESS-Huffman stream");
  std::string portableRestored;
  error.clear();
  expect(rocket::platform_compression::decompressXpress(
             windowsCompressed, portableRestored, error),
         "portable decoder accepts the Windows XPRESS-Huffman stream: " +
             error);
  expect(portableRestored == input,
         "portable decoder reproduces Windows XPRESS-Huffman input");
}
#endif

} // namespace

int main() {
  roundTrip({}, "empty input");
  roundTrip("Rocket compression round trip Rocket compression round trip",
            "text input");

  std::string bytes;
  for (int value = 0; value < 256; ++value)
    bytes.push_back(static_cast<char>(value));
  roundTrip(bytes, "all byte values");

  for (const std::size_t size : {65535U, 65536U, 65537U, 131073U}) {
    std::string boundary(size, '\0');
    for (std::size_t index = 0; index < size; ++index)
      boundary[index] = static_cast<char>((index * 131U + 255U) & 0xffU);
    roundTrip(boundary, "64 KiB block boundary " + std::to_string(size));
  }

  std::string output;
  std::string error;
  expect(!rocket::platform_compression::decompressXpress(
             "not compressed", output, error) && !error.empty(),
         "invalid input is rejected with an error");

#ifdef _WIN32
  windowsInteroperability(std::string(1024, 'A'));
#endif

  if (failures != 0) {
    std::cerr << failures << " compression test(s) failed\n";
    return 1;
  }
  std::cout << "compression tests passed\n";
  return 0;
}
