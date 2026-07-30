#pragma once

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace rocket::platform_datetime {

inline bool leapYear(std::int64_t year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

inline bool daysInMonth(std::int64_t year, std::int64_t month,
                        std::int64_t& days, std::string& error) {
  if (year < 1 || year > 9999 || month < 1 || month > 12) {
    error = "calendar year must be 1 through 9999 and month 1 through 12";
    return false;
  }
  constexpr std::int64_t lengths[] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
  days = lengths[month - 1] + (month == 2 && leapYear(year) ? 1 : 0);
  return true;
}

inline bool validDate(std::int64_t year, std::int64_t month, std::int64_t day,
                      std::string& error) {
  std::int64_t maximum = 0;
  return daysInMonth(year, month, maximum, error) && day >= 1 && day <= maximum
             ? true
             : (error.empty() ? (error = "calendar day is outside the selected month", false)
                              : false);
}

inline bool weekday(std::int64_t year, std::int64_t month, std::int64_t day,
                    std::int64_t& result, std::string& error) {
  if (!validDate(year, month, day, error)) return false;
  if (month < 3) { month += 12; --year; }
  const std::int64_t century = year / 100;
  const std::int64_t within = year % 100;
  const std::int64_t h = (day + 13 * (month + 1) / 5 + within + within / 4 +
                          century / 4 + 5 * century) % 7;
  result = (h + 6) % 7;
  return true;
}

#ifdef _WIN32
inline bool fileTime(std::int64_t unixMilliseconds, FILETIME& file,
                     std::string& error) {
  constexpr std::uint64_t epoch = 116444736000000000ULL;
  constexpr std::int64_t minimum = -11644473600000LL;
  constexpr std::int64_t maximum = 253402300799999LL;
  if (unixMilliseconds < minimum || unixMilliseconds > maximum) {
    error = "timestamp is outside the Windows calendar range";
    return false;
  }
  const std::uint64_t ticks = epoch +
      static_cast<std::uint64_t>(unixMilliseconds - minimum) * 10000ULL -
      static_cast<std::uint64_t>(-minimum) * 10000ULL;
  ULARGE_INTEGER value{};
  value.QuadPart = ticks;
  file.dwLowDateTime = value.LowPart;
  file.dwHighDateTime = value.HighPart;
  return true;
}

inline std::int64_t unixMilliseconds(const FILETIME& file) {
  ULARGE_INTEGER value{};
  value.LowPart = file.dwLowDateTime;
  value.HighPart = file.dwHighDateTime;
  constexpr std::uint64_t epoch = 116444736000000000ULL;
  if (value.QuadPart >= epoch)
    return static_cast<std::int64_t>((value.QuadPart - epoch) / 10000ULL);
  return -static_cast<std::int64_t>((epoch - value.QuadPart + 9999ULL) / 10000ULL);
}

inline std::string wideToUtf8(std::wstring_view input) {
  if (input.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                          input.data(), static_cast<int>(input.size()),
                                          nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(),
                          static_cast<int>(input.size()), result.data(), length,
                          nullptr, nullptr) <= 0)
    return {};
  return result;
}
#endif

inline bool formatUtc(std::int64_t unixMilliseconds, std::string& result,
                      std::string& error) {
#ifdef _WIN32
  FILETIME file{};
  SYSTEMTIME time{};
  if (!fileTime(unixMilliseconds, file, error) || !FileTimeToSystemTime(&file, &time)) {
    if (error.empty()) error = "could not convert timestamp to UTC calendar time";
    return false;
  }
  char text[32]{};
  const int written = std::snprintf(
      text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
      time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
      time.wSecond, time.wMilliseconds);
  if (written != 24) {
    error = "could not format UTC timestamp";
    return false;
  }
  result.assign(text, static_cast<std::size_t>(written));
  return true;
#else
  (void)unixMilliseconds; (void)result;
  error = "calendar conversion is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool parseFixed(std::string_view text, std::size_t start, std::size_t length,
                       unsigned& result) {
  result = 0;
  if (start + length > text.size()) return false;
  for (std::size_t index = start; index < start + length; ++index) {
    if (text[index] < '0' || text[index] > '9') return false;
    result = result * 10 + static_cast<unsigned>(text[index] - '0');
  }
  return true;
}

inline bool parseUtc(std::string_view text, std::int64_t& result,
                     std::string& error) {
  if (text.size() != 24 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
      text[13] != ':' || text[16] != ':' || text[19] != '.' || text[23] != 'Z') {
    error = "UTC timestamp must use YYYY-MM-DDTHH:MM:SS.mmmZ";
    return false;
  }
  unsigned year, month, day, hour, minute, second, millisecond;
  if (!parseFixed(text, 0, 4, year) || !parseFixed(text, 5, 2, month) ||
      !parseFixed(text, 8, 2, day) || !parseFixed(text, 11, 2, hour) ||
      !parseFixed(text, 14, 2, minute) || !parseFixed(text, 17, 2, second) ||
      !parseFixed(text, 20, 3, millisecond) || hour > 23 || minute > 59 ||
      second > 59) {
    error = "UTC timestamp contains an invalid field";
    return false;
  }
  std::string dateError;
  if (!validDate(year, month, day, dateError)) { error = dateError; return false; }
#ifdef _WIN32
  SYSTEMTIME time{};
  time.wYear = static_cast<WORD>(year);
  time.wMonth = static_cast<WORD>(month);
  time.wDay = static_cast<WORD>(day);
  time.wHour = static_cast<WORD>(hour);
  time.wMinute = static_cast<WORD>(minute);
  time.wSecond = static_cast<WORD>(second);
  time.wMilliseconds = static_cast<WORD>(millisecond);
  FILETIME file{};
  if (!SystemTimeToFileTime(&time, &file)) {
    error = "UTC timestamp is outside the Windows calendar range";
    return false;
  }
  result = unixMilliseconds(file);
  return true;
#else
  (void)result;
  error = "calendar conversion is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool localOffsetMinutes(std::int64_t unixMs, std::int64_t& result,
                               std::string& error) {
#ifdef _WIN32
  FILETIME file{};
  SYSTEMTIME utc{};
  SYSTEMTIME local{};
  if (!fileTime(unixMs, file, error) || !FileTimeToSystemTime(&file, &utc) ||
      !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) {
    if (error.empty()) error = "could not apply the Windows time-zone rules";
    return false;
  }
  FILETIME utcAsFile{};
  FILETIME localAsFile{};
  if (!SystemTimeToFileTime(&utc, &utcAsFile) ||
      !SystemTimeToFileTime(&local, &localAsFile)) {
    error = "could not calculate the local UTC offset";
    return false;
  }
  result = (unixMilliseconds(localAsFile) - unixMilliseconds(utcAsFile)) / 60000;
  return true;
#else
  (void)unixMs; (void)result;
  error = "time-zone conversion is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool timezoneName(std::string& result, std::string& error) {
#ifdef _WIN32
  DYNAMIC_TIME_ZONE_INFORMATION information{};
  if (GetDynamicTimeZoneInformation(&information) == TIME_ZONE_ID_INVALID) {
    error = "could not read the Windows time-zone configuration";
    return false;
  }
  std::wstring_view name(information.TimeZoneKeyName);
  if (name.empty()) name = information.StandardName;
  result = wideToUtf8(name);
  if (result.empty()) {
    error = "Windows time-zone name is unavailable";
    return false;
  }
  return true;
#else
  (void)result;
  error = "time-zone names are currently supported on Windows x64 only";
  return false;
#endif
}

} // namespace rocket::platform_datetime
