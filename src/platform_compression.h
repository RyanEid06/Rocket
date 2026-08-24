#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rocket::platform_compression {
namespace detail {

constexpr std::size_t MaximumUncompressedBytes = 64U * 1024U * 1024U;
constexpr std::size_t MaximumCompressedBytes = 80U * 1024U * 1024U;
constexpr std::size_t BlockBytes = 64U * 1024U;
constexpr std::size_t SymbolCount = 512;
constexpr unsigned MaximumCodeLength = 15;

// A deterministic literal-only code is still a conforming MS-XCA
// LZ77+Huffman stream. The first 255 literals use eight bits; literal 255 and
// the end marker use nine. The decoder below accepts the complete match
// surface, so streams produced by the Windows Compression API remain readable.
inline std::array<std::uint8_t, SymbolCount> literalCodeLengths() {
  std::array<std::uint8_t, SymbolCount> lengths{};
  for (std::size_t symbol = 0; symbol < 255; ++symbol) lengths[symbol] = 8;
  lengths[255] = 9;
  lengths[256] = 9;
  return lengths;
}

struct CanonicalCodes {
  std::array<std::uint16_t, SymbolCount> code{};
  std::array<std::uint8_t, SymbolCount> length{};
  std::array<std::uint16_t, MaximumCodeLength + 1> firstCode{};
  std::array<std::uint16_t, MaximumCodeLength + 1> firstSymbol{};
  std::array<std::uint16_t, MaximumCodeLength + 1> count{};
  std::array<std::uint16_t, SymbolCount> orderedSymbols{};
};

inline bool buildCanonicalCodes(
    const std::array<std::uint8_t, SymbolCount>& lengths,
    CanonicalCodes& result, std::string& error) {
  result = {};
  result.length = lengths;
  std::uint32_t scaledTotal = 0;
  for (std::size_t symbol = 0; symbol < SymbolCount; ++symbol) {
    const unsigned length = lengths[symbol];
    if (length > MaximumCodeLength) {
      error = "XPRESS Huffman table contains an invalid code length";
      return false;
    }
    if (length != 0) {
      ++result.count[length];
      scaledTotal += 1U << (MaximumCodeLength - length);
    }
  }
  if (scaledTotal != (1U << MaximumCodeLength)) {
    error = "XPRESS Huffman table is incomplete or oversubscribed";
    return false;
  }

  std::uint32_t nextCode = 0;
  std::uint16_t ordered = 0;
  for (unsigned length = 1; length <= MaximumCodeLength; ++length) {
    nextCode = (nextCode + result.count[length - 1]) << 1;
    result.firstCode[length] = static_cast<std::uint16_t>(nextCode);
    result.firstSymbol[length] = ordered;
    std::uint32_t symbolCode = nextCode;
    for (std::size_t symbol = 0; symbol < SymbolCount; ++symbol) {
      if (lengths[symbol] != length) continue;
      result.code[symbol] = static_cast<std::uint16_t>(symbolCode++);
      result.orderedSymbols[ordered++] = static_cast<std::uint16_t>(symbol);
    }
  }
  return true;
}

inline void appendWord(std::string& output, const std::uint16_t value) {
  output.push_back(static_cast<char>(value & 0xffU));
  output.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

class BitWriter {
public:
  explicit BitWriter(std::string& output) : output_(output) {}

  bool write(unsigned count, std::uint32_t bits) {
    if (count > MaximumCodeLength) return false;
    for (unsigned remaining = count; remaining > 0; --remaining) {
      const unsigned bit = (bits >> (remaining - 1U)) & 1U;
      --freeBits_;
      word_ |= static_cast<std::uint16_t>(bit << freeBits_);
      if (freeBits_ == 0) {
        appendWord(output_, word_);
        word_ = 0;
        freeBits_ = 16;
      }
    }
    return output_.size() <= MaximumCompressedBytes;
  }

  bool finish() {
    if (freeBits_ != 16) appendWord(output_, word_);
    appendWord(output_, 0); // the format's required look-ahead word
    return output_.size() <= MaximumCompressedBytes;
  }

private:
  std::string& output_;
  std::uint16_t word_ = 0;
  unsigned freeBits_ = 16;
};

inline bool readWord(std::string_view input, std::size_t position,
                     std::uint16_t& value) {
  if (position > input.size() || input.size() - position < 2) return false;
  value = static_cast<std::uint8_t>(input[position]) |
          (static_cast<std::uint16_t>(
               static_cast<std::uint8_t>(input[position + 1]))
           << 8U);
  return true;
}

class BitReader {
public:
  BitReader(std::string_view input, std::size_t position, std::string& error)
      : input_(input), position_(position), error_(error) {
    std::uint16_t first = 0;
    std::uint16_t second = 0;
    if (!readWord(input_, position_, first) ||
        !readWord(input_, position_ + 2, second)) {
      error_ = "XPRESS Huffman bit stream is truncated";
      return;
    }
    nextBits_ = (static_cast<std::uint32_t>(first) << 16U) | second;
    position_ += 4;
    valid_ = true;
  }

  bool bits(unsigned count, std::uint32_t& value) {
    if (!valid_ || count > MaximumCodeLength) return false;
    if (count == 0) {
      value = 0;
      return true;
    }
    value = nextBits_ >> (32U - count);
    nextBits_ <<= count;
    extraBitCount_ -= static_cast<int>(count);
    if (extraBitCount_ < 0) {
      std::uint16_t word = 0;
      if (!readWord(input_, position_, word)) {
        error_ = "XPRESS Huffman bit stream is truncated";
        valid_ = false;
        return false;
      }
      nextBits_ |= static_cast<std::uint32_t>(word)
                   << static_cast<unsigned>(-extraBitCount_);
      extraBitCount_ += 16;
      position_ += 2;
    }
    return true;
  }

  bool byte(std::uint8_t& value) {
    if (!valid_ || position_ >= input_.size()) {
      error_ = "XPRESS Huffman match length is truncated";
      valid_ = false;
      return false;
    }
    value = static_cast<std::uint8_t>(input_[position_++]);
    return true;
  }

  bool word(std::uint16_t& value) {
    if (!valid_ || !readWord(input_, position_, value)) {
      error_ = "XPRESS Huffman match length is truncated";
      valid_ = false;
      return false;
    }
    position_ += 2;
    return true;
  }

  bool doubleWord(std::uint32_t& value) {
    std::uint16_t low = 0;
    std::uint16_t high = 0;
    if (!word(low) || !word(high)) return false;
    value = low | (static_cast<std::uint32_t>(high) << 16U);
    return true;
  }

  std::size_t position() const { return position_; }

private:
  std::string_view input_;
  std::size_t position_ = 0;
  std::string& error_;
  std::uint32_t nextBits_ = 0;
  int extraBitCount_ = 16;
  bool valid_ = false;
};

inline bool decodeSymbol(BitReader& reader, const CanonicalCodes& codes,
                         std::uint16_t& symbol) {
  std::uint32_t code = 0;
  for (unsigned length = 1; length <= MaximumCodeLength; ++length) {
    std::uint32_t bit = 0;
    if (!reader.bits(1, bit)) return false;
    code = (code << 1U) | bit;
    const std::uint32_t first = codes.firstCode[length];
    if (code < first || code - first >= codes.count[length]) continue;
    symbol = codes.orderedSymbols[codes.firstSymbol[length] + code - first];
    return true;
  }
  return false;
}

inline bool readCodeLengths(std::string_view input, std::size_t position,
                            std::array<std::uint8_t, SymbolCount>& lengths) {
  if (position > input.size() || input.size() - position < 256) return false;
  for (std::size_t index = 0; index < 256; ++index) {
    const auto byte = static_cast<std::uint8_t>(input[position + index]);
    lengths[index * 2] = byte & 0x0fU;
    lengths[index * 2 + 1] = byte >> 4U;
  }
  return true;
}

inline void appendLittle(std::string& output, std::uint64_t value,
                         unsigned bytes) {
  for (unsigned index = 0; index < bytes; ++index)
    output.push_back(static_cast<char>((value >> (index * 8U)) & 0xffU));
}

inline bool readLittle(std::string_view input, std::size_t position,
                       unsigned bytes, std::uint64_t& value) {
  if (bytes > 8 || position > input.size() ||
      input.size() - position < bytes) {
    return false;
  }
  value = 0;
  for (unsigned index = 0; index < bytes; ++index) {
    value |= static_cast<std::uint64_t>(
                 static_cast<std::uint8_t>(input[position + index]))
             << (index * 8U);
  }
  return true;
}

inline std::uint32_t bufferHeaderCrc(std::string_view header) {
  std::uint32_t crc = 0;
  for (std::size_t index = 0; index < header.size(); ++index) {
    const std::uint8_t byte =
        index == 6 ? 0 : static_cast<std::uint8_t>(header[index]);
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc & 1U) != 0 ? (crc >> 1U) ^ 0xedb88320U : crc >> 1U;
  }
  return crc;
}

} // namespace detail

inline bool compressRawXpress(std::string_view input, std::string& output,
                              std::string& error) {
  output.clear();
  error.clear();
  if (input.size() > detail::MaximumUncompressedBytes) {
    error = "compression input exceeds the 64 MiB limit";
    return false;
  }

  const auto lengths = detail::literalCodeLengths();
  detail::CanonicalCodes codes;
  if (!detail::buildCanonicalCodes(lengths, codes, error)) return false;

  std::size_t offset = 0;
  bool emittedEndMarker = false;
  do {
    for (std::size_t index = 0; index < 256; ++index) {
      output.push_back(static_cast<char>(lengths[index * 2] |
          static_cast<std::uint8_t>(lengths[index * 2 + 1] << 4U)));
    }
    detail::BitWriter writer(output);
    const std::size_t count =
        std::min(detail::BlockBytes, input.size() - offset);
    for (std::size_t index = 0; index < count; ++index) {
      const auto symbol = static_cast<std::uint8_t>(input[offset + index]);
      if (!writer.write(codes.length[symbol], codes.code[symbol])) {
        output.clear();
        error = "compressed output exceeds the 80 MiB safety limit";
        return false;
      }
    }
    offset += count;
    const bool finalBlock = offset == input.size() && count < detail::BlockBytes;
    if (finalBlock &&
        !writer.write(codes.length[256], codes.code[256])) {
      output.clear();
      error = "compressed output exceeds the 80 MiB safety limit";
      return false;
    }
    if (!writer.finish()) {
      output.clear();
      error = "compressed output exceeds the 80 MiB safety limit";
      return false;
    }
    emittedEndMarker = finalBlock;
  } while (!emittedEndMarker);
  return true;
}

inline bool decompressRawXpress(std::string_view input, std::string& output,
                                std::string& error) {
  output.clear();
  error.clear();
  if (input.size() > detail::MaximumCompressedBytes) {
    error = "compressed input exceeds the 80 MiB safety limit";
    return false;
  }

  std::size_t position = 0;
  while (true) {
    std::array<std::uint8_t, detail::SymbolCount> lengths{};
    if (!detail::readCodeLengths(input, position, lengths)) {
      error = output.empty()
                  ? "compressed data is invalid or truncated"
                  : "XPRESS Huffman stream ended before its end marker";
      output.clear();
      return false;
    }
    detail::CanonicalCodes codes;
    if (!detail::buildCanonicalCodes(lengths, codes, error)) {
      output.clear();
      return false;
    }
    detail::BitReader reader(input, position + 256, error);
    const std::size_t blockStart = output.size();
    const std::size_t blockEnd = blockStart + detail::BlockBytes;

    while (output.size() < blockEnd) {
      std::uint16_t symbol = 0;
      if (!detail::decodeSymbol(reader, codes, symbol)) {
        if (error.empty()) error = "XPRESS Huffman symbol is invalid";
        output.clear();
        return false;
      }
      if (symbol < 256) {
        if (output.size() == detail::MaximumUncompressedBytes) {
          error = "decompressed output exceeds the 64 MiB limit";
          output.clear();
          return false;
        }
        output.push_back(static_cast<char>(symbol));
        continue;
      }
      if (symbol == 256 && reader.position() >= input.size()) return true;

      const unsigned match = symbol - 256U;
      const unsigned offsetBitCount = match / 16U;
      std::uint64_t matchLength = match % 16U;
      if (matchLength == 15) {
        std::uint8_t extra = 0;
        if (!reader.byte(extra)) {
          output.clear();
          return false;
        }
        if (extra < 255) {
          matchLength = static_cast<std::uint64_t>(extra) + 18U;
        } else {
          std::uint16_t extended = 0;
          if (!reader.word(extended)) {
            output.clear();
            return false;
          }
          std::uint32_t fullLength = extended;
          if (extended == 0 && !reader.doubleWord(fullLength)) {
            output.clear();
            return false;
          }
          if (fullLength < 15) {
            error = "XPRESS Huffman match length is invalid";
            output.clear();
            return false;
          }
          matchLength = static_cast<std::uint64_t>(fullLength) + 3U;
        }
      } else {
        matchLength += 3U;
      }

      std::uint32_t offsetBits = 0;
      if (!reader.bits(offsetBitCount, offsetBits)) {
        output.clear();
        return false;
      }
      const std::uint64_t matchOffset =
          (std::uint64_t{1} << offsetBitCount) + offsetBits;
      if (matchOffset > output.size() ||
          matchLength > blockEnd - output.size() ||
          matchLength > detail::MaximumUncompressedBytes - output.size()) {
        error = "XPRESS Huffman match exceeds the available output window";
        output.clear();
        return false;
      }
      for (std::uint64_t index = 0; index < matchLength; ++index) {
        output.push_back(output[output.size() -
                                static_cast<std::size_t>(matchOffset)]);
      }
    }

    position = reader.position();
    if (position >= input.size()) return true;
    if (output.size() == detail::MaximumUncompressedBytes) {
      error = "decompressed output exceeds the 64 MiB limit";
      output.clear();
      return false;
    }
  }
}

// The public Rocket 1.5 API used Cabinet.dll buffer mode on Windows. Preserve
// that portable wire format: a checksummed 24-byte header followed by sized
// XPRESS-Huffman or stored blocks. Emitting a stored block is a valid buffer-
// mode result when compression would not reduce the payload, and avoids a
// platform-dependent encoder while remaining readable by Cabinet.dll.
inline bool compressXpress(std::string_view input, std::string& output,
                           std::string& error) {
  output.clear();
  error.clear();
  if (input.size() > detail::MaximumUncompressedBytes) {
    error = "compression input exceeds the 64 MiB limit";
    return false;
  }
  detail::appendLittle(output, 0xc0e5510aU, 4);
  detail::appendLittle(output, 24, 2);
  output.push_back('\0'); // checksum is filled after the fixed header exists
  output.push_back('\4'); // COMPRESS_ALGORITHM_XPRESS_HUFF
  detail::appendLittle(output, input.size(), 8);
  detail::appendLittle(output, input.size(), 8);
  output[6] = static_cast<char>(detail::bufferHeaderCrc(output) & 0xffU);
  if (!input.empty()) {
    detail::appendLittle(output, input.size(), 4);
    output.append(input.data(), input.size());
  }
  return true;
}

inline bool decompressXpress(std::string_view input, std::string& output,
                             std::string& error) {
  output.clear();
  error.clear();
  std::uint64_t magic = 0;
  if (!detail::readLittle(input, 0, 4, magic) || magic != 0xc0e5510aU)
    return decompressRawXpress(input, output, error);

  std::uint64_t headerSize = 0;
  std::uint64_t totalSize = 0;
  std::uint64_t blockSize = 0;
  if (!detail::readLittle(input, 4, 2, headerSize) || headerSize != 24 ||
      input.size() < headerSize ||
      static_cast<std::uint8_t>(input[7]) != 4 ||
      !detail::readLittle(input, 8, 8, totalSize) ||
      !detail::readLittle(input, 16, 8, blockSize) ||
      totalSize > detail::MaximumUncompressedBytes ||
      (totalSize != 0 && (blockSize == 0 || blockSize > totalSize)) ||
      (detail::bufferHeaderCrc(input.substr(0, 24)) & 0xffU) !=
          static_cast<std::uint8_t>(input[6])) {
    error = "XPRESS Huffman buffer header is invalid";
    return false;
  }
  if (totalSize == 0) {
    if (input.size() != 24) {
      error = "XPRESS Huffman empty buffer has trailing data";
      return false;
    }
    return true;
  }

  output.reserve(static_cast<std::size_t>(totalSize));
  std::size_t position = 24;
  while (output.size() < totalSize) {
    std::uint64_t storedSize = 0;
    if (!detail::readLittle(input, position, 4, storedSize) ||
        storedSize > detail::MaximumCompressedBytes) {
      output.clear();
      error = "XPRESS Huffman buffer block header is invalid or truncated";
      return false;
    }
    position += 4;
    const std::size_t expected = static_cast<std::size_t>(std::min<std::uint64_t>(
        blockSize, totalSize - output.size()));
    if (storedSize == 0 || position > input.size() ||
        storedSize > input.size() - position || storedSize > expected) {
      output.clear();
      error = "XPRESS Huffman buffer block size is invalid";
      return false;
    }
    const std::string_view block = input.substr(
        position, static_cast<std::size_t>(storedSize));
    if (storedSize == expected) {
      output.append(block.data(), block.size());
    } else {
      std::string restored;
      std::string blockError;
      if (!decompressRawXpress(block, restored, blockError) ||
          restored.size() != expected) {
        output.clear();
        error = blockError.empty()
                    ? "XPRESS Huffman buffer block has the wrong output size"
                    : blockError;
        return false;
      }
      output += restored;
    }
    position += static_cast<std::size_t>(storedSize);
  }
  if (position != input.size()) {
    output.clear();
    error = "XPRESS Huffman buffer has trailing data";
    return false;
  }
  return true;
}

} // namespace rocket::platform_compression
