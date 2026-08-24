#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#ifdef ROCKET_HAS_ICU
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#endif

namespace rocket::platform_unicode {

enum class NormalizationForm { Nfc, Nfd };

inline bool normalize(std::string_view input, NormalizationForm form,
                      std::string& output, std::string& error) {
#ifdef ROCKET_HAS_ICU
  if (input.empty()) {
    output.clear();
    return true;
  }
  if (input.size() > static_cast<std::size_t>(
                         std::numeric_limits<std::int32_t>::max())) {
    error = "Unicode input exceeds the ICU length limit";
    return false;
  }
  UErrorCode status = U_ZERO_ERROR;
  std::int32_t utf16Length = 0;
  u_strFromUTF8(nullptr, 0, &utf16Length, input.data(),
                static_cast<std::int32_t>(input.size()), &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    error = "invalid UTF-8 text";
    return false;
  }
  status = U_ZERO_ERROR;
  std::vector<UChar> utf16(static_cast<std::size_t>(utf16Length));
  u_strFromUTF8(utf16.data(), utf16Length, nullptr, input.data(),
                static_cast<std::int32_t>(input.size()), &status);
  if (U_FAILURE(status)) {
    error = "invalid UTF-8 text";
    return false;
  }
  const UNormalizer2* normalizer =
      form == NormalizationForm::Nfc ? unorm2_getNFCInstance(&status)
                                     : unorm2_getNFDInstance(&status);
  if (U_FAILURE(status) || !normalizer) {
    error = "ICU Unicode normalization is unavailable";
    return false;
  }
  status = U_ZERO_ERROR;
  const std::int32_t normalizedLength = unorm2_normalize(
      normalizer, utf16.data(), utf16Length, nullptr, 0, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    error = "Unicode normalization failed";
    return false;
  }
  status = U_ZERO_ERROR;
  std::vector<UChar> normalized(static_cast<std::size_t>(normalizedLength));
  unorm2_normalize(normalizer, utf16.data(), utf16Length, normalized.data(),
                   normalizedLength, &status);
  if (U_FAILURE(status)) {
    error = "Unicode normalization failed";
    return false;
  }
  status = U_ZERO_ERROR;
  std::int32_t utf8Length = 0;
  u_strToUTF8(nullptr, 0, &utf8Length, normalized.data(), normalizedLength,
              &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    error = "Unicode normalization output is invalid";
    return false;
  }
  status = U_ZERO_ERROR;
  output.resize(static_cast<std::size_t>(utf8Length));
  u_strToUTF8(output.data(), utf8Length, nullptr, normalized.data(),
              normalizedLength, &status);
  if (U_FAILURE(status)) {
    output.clear();
    error = "Unicode normalization output is invalid";
    return false;
  }
  return true;
#else
  (void)input;
  (void)form;
  (void)output;
  error = "Unicode normalization requires the bundled ICU provider";
  return false;
#endif
}

} // namespace rocket::platform_unicode
