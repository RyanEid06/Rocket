#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace rocket::safe_archive {

struct Entry {
  std::string name;
  std::string bytes;
};

namespace detail {

inline std::string asciiLower(std::string_view value) {
  std::string result(value);
  for (char& character : result)
    if (character >= 'A' && character <= 'Z')
      character = static_cast<char>(character - 'A' + 'a');
  return result;
}

inline bool windowsDeviceName(std::string_view component) {
  std::string stem(component.substr(0, component.find('.')));
  stem = asciiLower(stem);
  if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul")
    return true;
  if (stem.size() == 4 &&
      (stem.starts_with("com") || stem.starts_with("lpt")) &&
      stem[3] >= '1' && stem[3] <= '9')
    return true;
  return false;
}

inline bool safeName(std::string_view name) {
  if (name.empty() || name.size() > 100 || name.front() == '/' ||
      name.front() == '\\' || name.find('\\') != std::string_view::npos ||
      name.find(':') != std::string_view::npos) return false;
  std::size_t start = 0;
  while (start <= name.size()) {
    const std::size_t end = name.find('/', start);
    const std::string_view part = name.substr(
        start, end == std::string_view::npos ? std::string_view::npos : end - start);
    if (part.empty() || part == "." || part == ".." ||
        part.back() == '.' || part.back() == ' ' || windowsDeviceName(part))
      return false;
    for (const unsigned char byte : part)
      if (byte < 0x20 || byte == 0x7f) return false;
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return true;
}

inline bool octal(char* destination, std::size_t width, std::uint64_t value) {
  std::string digits;
  do {
    digits.insert(digits.begin(), static_cast<char>('0' + (value & 7)));
    value >>= 3;
  } while (value != 0);
  if (digits.size() + 1 > width) return false;
  std::fill(destination, destination + width, '0');
  std::copy(digits.begin(), digits.end(), destination + width - 1 - digits.size());
  destination[width - 1] = '\0';
  return true;
}

inline bool parseOctal(const char* source, std::size_t width, std::uint64_t& value) {
  value = 0;
  std::size_t index = 0;
  while (index < width && (source[index] == ' ' || source[index] == '\0')) ++index;
  bool digit = false;
  for (; index < width && source[index] != '\0' && source[index] != ' '; ++index) {
    if (source[index] < '0' || source[index] > '7') return false;
    digit = true;
    if (value > (UINT64_MAX - static_cast<unsigned>(source[index] - '0')) / 8)
      return false;
    value = value * 8 + static_cast<unsigned>(source[index] - '0');
  }
  for (; index < width; ++index)
    if (source[index] != '\0' && source[index] != ' ') return false;
  return digit;
}

inline std::filesystem::path path(std::string_view utf8) {
  const std::u8string value(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
  return std::filesystem::path(value);
}

inline bool read(std::string_view archivePath, std::vector<Entry>& entries,
                 std::string& error) {
  entries.clear();
  std::ifstream input(path(archivePath), std::ios::binary);
  if (!input) { error = "could not open TAR archive"; return false; }
  input.seekg(0, std::ios::end);
  const std::streamoff size = input.tellg();
  if (size < 0 || size > 128 * 1024 * 1024 || size % 512 != 0) {
    error = "TAR archive size is invalid or exceeds 128 MiB";
    return false;
  }
  input.seekg(0);
  std::uint64_t consumed = 0;
  std::uint64_t totalBytes = 0;
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> foldedNames;
  while (consumed + 512 <= static_cast<std::uint64_t>(size)) {
    std::array<char, 512> header{};
    input.read(header.data(), header.size());
    if (!input) { error = "TAR archive header is truncated"; return false; }
    consumed += 512;
    bool zero = true;
    for (char byte : header) if (byte != '\0') { zero = false; break; }
    if (zero) {
      bool secondEndBlock = false;
      while (consumed + 512 <= static_cast<std::uint64_t>(size)) {
        input.read(header.data(), header.size());
        if (!input) { error = "TAR end marker is truncated"; return false; }
        consumed += 512;
        secondEndBlock = true;
        for (char byte : header) {
          if (byte != '\0') {
            error = "TAR archive contains data after its end marker";
            return false;
          }
        }
      }
      if (!secondEndBlock) {
        error = "TAR archive is missing its second end block";
        return false;
      }
      return true;
    }
    if (std::string_view(header.data() + 257, 6) !=
            std::string_view("ustar\0", 6) ||
        header[263] != '0' || header[264] != '0') {
      error = "TAR entry does not use the ustar format";
      return false;
    }
    if (std::any_of(header.begin() + 345, header.begin() + 500,
                    [](char byte) { return byte != '\0'; })) {
      error = "TAR ustar prefix fields are not supported";
      return false;
    }
    std::uint64_t recordedChecksum = 0;
    if (!parseOctal(header.data() + 148, 8, recordedChecksum)) {
      error = "TAR entry checksum is invalid";
      return false;
    }
    std::uint64_t actualChecksum = 0;
    for (std::size_t index = 0; index < header.size(); ++index)
      actualChecksum += index >= 148 && index < 156
                            ? static_cast<unsigned>(' ')
                            : static_cast<unsigned char>(header[index]);
    if (actualChecksum != recordedChecksum) {
      error = "TAR entry checksum mismatch";
      return false;
    }
    const auto nameEnd = std::find(header.begin(), header.begin() + 100, '\0');
    const std::size_t nameLength = static_cast<std::size_t>(nameEnd - header.begin());
    const std::string name(header.data(), nameLength);
    if (!safeName(name) || !names.emplace(name).second ||
        !foldedNames.emplace(asciiLower(name)).second) {
      error = "TAR entry name is unsafe, duplicated, or case-colliding";
      return false;
    }
    if (header[156] != '\0' && header[156] != '0') {
      error = "TAR archive contains a non-regular entry";
      return false;
    }
    std::uint64_t entrySize = 0;
    if (!parseOctal(header.data() + 124, 12, entrySize) ||
        entrySize > 64 * 1024 * 1024 || totalBytes + entrySize > 64 * 1024 * 1024) {
      error = "TAR entry size is invalid or exceeds the 64 MiB content limit";
      return false;
    }
    const std::uint64_t padded = (entrySize + 511) / 512 * 512;
    if (padded > static_cast<std::uint64_t>(size) - consumed) {
      error = "TAR entry body is truncated";
      return false;
    }
    Entry entry{name, std::string(static_cast<std::size_t>(entrySize), '\0')};
    if (entrySize) input.read(entry.bytes.data(), static_cast<std::streamsize>(entrySize));
    if (!input) { error = "TAR entry body could not be read"; return false; }
    if (padded > entrySize)
      input.seekg(static_cast<std::streamoff>(padded - entrySize), std::ios::cur);
    consumed += padded;
    totalBytes += entrySize;
    entries.push_back(std::move(entry));
    if (entries.size() > 1024) {
      error = "TAR archive contains more than 1024 entries";
      return false;
    }
  }
  error = "TAR archive is missing its end marker";
  return false;
}

} // namespace detail

inline bool create(std::string_view archivePath, const std::vector<Entry>& entries,
                   std::string& error) {
  if (entries.empty() || entries.size() > 1024) {
    error = "TAR creation requires 1 through 1024 entries";
    return false;
  }
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> foldedNames;
  std::uint64_t totalBytes = 0;
  for (const Entry& entry : entries) {
    if (!detail::safeName(entry.name) || !names.emplace(entry.name).second ||
        !foldedNames.emplace(detail::asciiLower(entry.name)).second) {
      error = "TAR entry name is unsafe, duplicated, or case-colliding";
      return false;
    }
    totalBytes += entry.bytes.size();
    if (totalBytes > 64 * 1024 * 1024) {
      error = "TAR contents exceed the 64 MiB limit";
      return false;
    }
  }
  std::ofstream output(detail::path(archivePath), std::ios::binary | std::ios::trunc);
  if (!output) { error = "could not create TAR archive"; return false; }
  for (const Entry& entry : entries) {
    std::array<char, 512> header{};
    std::copy(entry.name.begin(), entry.name.end(), header.begin());
    if (!detail::octal(header.data() + 100, 8, 0644) ||
        !detail::octal(header.data() + 108, 8, 0) ||
        !detail::octal(header.data() + 116, 8, 0) ||
        !detail::octal(header.data() + 124, 12, entry.bytes.size()) ||
        !detail::octal(header.data() + 136, 12, 0)) {
      error = "TAR entry metadata exceeds the ustar field width";
      return false;
    }
    std::fill(header.begin() + 148, header.begin() + 156, ' ');
    header[156] = '0';
    std::copy_n("ustar", 5, header.begin() + 257);
    header[262] = '\0';
    header[263] = '0';
    header[264] = '0';
    std::uint64_t checksum = 0;
    for (char byte : header) checksum += static_cast<unsigned char>(byte);
    if (checksum > 0777777 ||
        std::snprintf(header.data() + 148, 7, "%06llo",
                      static_cast<unsigned long long>(checksum)) != 6) {
      error = "TAR checksum exceeds the ustar field width";
      return false;
    }
    header[155] = ' ';
    output.write(header.data(), header.size());
    if (!entry.bytes.empty())
      output.write(entry.bytes.data(), static_cast<std::streamsize>(entry.bytes.size()));
    const std::size_t padding = (512 - entry.bytes.size() % 512) % 512;
    if (padding) {
      const std::array<char, 512> zero{};
      output.write(zero.data(), static_cast<std::streamsize>(padding));
    }
  }
  const std::array<char, 1024> end{};
  output.write(end.data(), end.size());
  output.flush();
  if (!output) { error = "could not finish TAR archive"; return false; }
  return true;
}

inline bool list(std::string_view archivePath, std::vector<std::string>& names,
                 std::string& error) {
  std::vector<Entry> entries;
  if (!detail::read(archivePath, entries, error)) return false;
  names.reserve(entries.size());
  for (Entry& entry : entries) names.push_back(std::move(entry.name));
  return true;
}

inline bool read(std::string_view archivePath, std::string_view requested,
                 std::string& bytes, std::string& error) {
  if (!detail::safeName(requested)) {
    error = "requested TAR entry name is unsafe";
    return false;
  }
  std::vector<Entry> entries;
  if (!detail::read(archivePath, entries, error)) return false;
  for (Entry& entry : entries)
    if (entry.name == requested) { bytes = std::move(entry.bytes); return true; }
  error = "TAR entry was not found";
  return false;
}

inline bool readAll(std::string_view archivePath, std::vector<Entry>& entries,
                    std::string& error) {
  return detail::read(archivePath, entries, error);
}

inline bool validEntryName(std::string_view name) {
  return detail::safeName(name);
}

} // namespace rocket::safe_archive
