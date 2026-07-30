#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#endif

namespace rocket::platform_net {

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalidSocket = INVALID_SOCKET;

namespace detail {

struct WinsockApi {
  HMODULE library = LoadLibraryW(L"ws2_32.dll");
  decltype(&WSAStartup) startup = nullptr;
  decltype(&WSACleanup) cleanup = nullptr;
  decltype(&WSAGetLastError) lastError = nullptr;
  decltype(&getaddrinfo) getAddressInfo = nullptr;
  decltype(&freeaddrinfo) freeAddressInfo = nullptr;
  decltype(&getnameinfo) getNameInfo = nullptr;
  decltype(&socket) createSocket = nullptr;
  decltype(&connect) connectSocket = nullptr;
  decltype(&bind) bindSocket = nullptr;
  decltype(&listen) listenSocket = nullptr;
  decltype(&accept) acceptSocket = nullptr;
  decltype(&send) sendSocket = nullptr;
  decltype(&recv) receiveSocket = nullptr;
  decltype(&closesocket) closeSocket = nullptr;
  decltype(&select) selectSocket = nullptr;
  decltype(&setsockopt) setSocketOption = nullptr;
  decltype(&getsockopt) getSocketOption = nullptr;
  decltype(&getsockname) getSocketName = nullptr;
  decltype(&ioctlsocket) controlSocket = nullptr;
  bool started = false;

  WinsockApi() {
    if (!library) return;
#define ROCKET_WINSOCK(NAME, MEMBER) \
    MEMBER = reinterpret_cast<decltype(MEMBER)>(GetProcAddress(library, NAME))
    ROCKET_WINSOCK("WSAStartup", startup);
    ROCKET_WINSOCK("WSACleanup", cleanup);
    ROCKET_WINSOCK("WSAGetLastError", lastError);
    ROCKET_WINSOCK("getaddrinfo", getAddressInfo);
    ROCKET_WINSOCK("freeaddrinfo", freeAddressInfo);
    ROCKET_WINSOCK("getnameinfo", getNameInfo);
    ROCKET_WINSOCK("socket", createSocket);
    ROCKET_WINSOCK("connect", connectSocket);
    ROCKET_WINSOCK("bind", bindSocket);
    ROCKET_WINSOCK("listen", listenSocket);
    ROCKET_WINSOCK("accept", acceptSocket);
    ROCKET_WINSOCK("send", sendSocket);
    ROCKET_WINSOCK("recv", receiveSocket);
    ROCKET_WINSOCK("closesocket", closeSocket);
    ROCKET_WINSOCK("select", selectSocket);
    ROCKET_WINSOCK("setsockopt", setSocketOption);
    ROCKET_WINSOCK("getsockopt", getSocketOption);
    ROCKET_WINSOCK("getsockname", getSocketName);
    ROCKET_WINSOCK("ioctlsocket", controlSocket);
#undef ROCKET_WINSOCK
    if (!complete()) return;
    WSADATA data{};
    started = startup(MAKEWORD(2, 2), &data) == 0;
  }

  ~WinsockApi() {
    if (started) cleanup();
    if (library) FreeLibrary(library);
  }

  bool complete() const {
    return library && startup && cleanup && lastError && getAddressInfo &&
           freeAddressInfo && getNameInfo && createSocket && connectSocket &&
           bindSocket && listenSocket && acceptSocket && sendSocket &&
           receiveSocket && closeSocket && selectSocket && setSocketOption &&
           getSocketOption && getSocketName && controlSocket;
  }
};

inline WinsockApi& winsock() {
  static WinsockApi value;
  return value;
}

inline bool ready(std::string& error) {
  if (winsock().complete() && winsock().started) return true;
  error = "Windows Winsock 2.2 is unavailable";
  return false;
}

inline std::string socketError(std::string_view operation) {
  return std::string(operation) + " failed with Winsock error " +
         std::to_string(winsock().lastError());
}

inline bool validTimeout(std::int64_t milliseconds, std::string& error) {
  if (milliseconds < 0 || milliseconds > 3600000) {
    error = "network timeout must be between 0 and 3600000 milliseconds";
    return false;
  }
  return true;
}

class Deadline {
public:
  explicit Deadline(std::int64_t milliseconds)
      : end_(std::chrono::steady_clock::now() +
             std::chrono::milliseconds(milliseconds)) {}

  std::int64_t remainingMilliseconds() const {
    const auto now = std::chrono::steady_clock::now();
    if (now >= end_) return 0;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_ - now);
    return (std::max)(std::int64_t{1}, remaining.count());
  }

private:
  std::chrono::steady_clock::time_point end_;
};

inline bool wait(Socket socket, bool writing, std::int64_t milliseconds,
                 std::string& error) {
  fd_set descriptor;
  FD_ZERO(&descriptor);
  FD_SET(socket, &descriptor);
  timeval timeout{static_cast<long>(milliseconds / 1000),
                  static_cast<long>((milliseconds % 1000) * 1000)};
  const int selected = winsock().selectSocket(
      0, writing ? nullptr : &descriptor, writing ? &descriptor : nullptr,
      nullptr, &timeout);
  if (selected > 0) return true;
  error = selected == 0 ? "network operation timed out" : socketError("select");
  return false;
}

inline std::wstring utf8ToWide(std::string_view input) {
  if (input.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          input.data(), static_cast<int>(input.size()),
                                          nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                          static_cast<int>(input.size()), result.data(), length) <= 0)
    return {};
  return result;
}

struct WinHttpApi {
  HMODULE library = LoadLibraryW(L"winhttp.dll");
  decltype(&WinHttpOpen) open = nullptr;
  decltype(&WinHttpCrackUrl) crackUrl = nullptr;
  decltype(&WinHttpConnect) connect = nullptr;
  decltype(&WinHttpOpenRequest) openRequest = nullptr;
  decltype(&WinHttpSetTimeouts) setTimeouts = nullptr;
  decltype(&WinHttpSendRequest) sendRequest = nullptr;
  decltype(&WinHttpReceiveResponse) receiveResponse = nullptr;
  decltype(&WinHttpQueryHeaders) queryHeaders = nullptr;
  decltype(&WinHttpQueryDataAvailable) queryAvailable = nullptr;
  decltype(&WinHttpReadData) readData = nullptr;
  decltype(&WinHttpCloseHandle) closeHandle = nullptr;

  WinHttpApi() {
    if (!library) return;
#define ROCKET_WINHTTP(NAME, MEMBER) \
    MEMBER = reinterpret_cast<decltype(MEMBER)>(GetProcAddress(library, NAME))
    ROCKET_WINHTTP("WinHttpOpen", open);
    ROCKET_WINHTTP("WinHttpCrackUrl", crackUrl);
    ROCKET_WINHTTP("WinHttpConnect", connect);
    ROCKET_WINHTTP("WinHttpOpenRequest", openRequest);
    ROCKET_WINHTTP("WinHttpSetTimeouts", setTimeouts);
    ROCKET_WINHTTP("WinHttpSendRequest", sendRequest);
    ROCKET_WINHTTP("WinHttpReceiveResponse", receiveResponse);
    ROCKET_WINHTTP("WinHttpQueryHeaders", queryHeaders);
    ROCKET_WINHTTP("WinHttpQueryDataAvailable", queryAvailable);
    ROCKET_WINHTTP("WinHttpReadData", readData);
    ROCKET_WINHTTP("WinHttpCloseHandle", closeHandle);
#undef ROCKET_WINHTTP
  }

  ~WinHttpApi() { if (library) FreeLibrary(library); }

  bool complete() const {
    return library && open && crackUrl && connect && openRequest && setTimeouts &&
           sendRequest && receiveResponse && queryHeaders && queryAvailable &&
           readData && closeHandle;
  }
};

inline std::string windowsError(std::string_view operation) {
  return std::string(operation) + " failed with Windows error " +
         std::to_string(GetLastError());
}

} // namespace detail

#else
using Socket = std::uintptr_t;
constexpr Socket invalidSocket = static_cast<Socket>(-1);
#endif

struct HttpResponse {
  std::int64_t status = 0;
  std::string body;
};

inline bool resolve(std::string_view host, std::string_view service,
                    std::vector<std::string>& addresses, std::string& error) {
#ifdef _WIN32
  if (host.empty() || service.empty()) {
    error = "DNS host and service must not be empty";
    return false;
  }
  if (!detail::ready(error)) return false;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo* result = nullptr;
  const std::string hostText(host);
  const std::string serviceText(service);
  const int status = detail::winsock().getAddressInfo(
      hostText.c_str(), serviceText.c_str(), &hints, &result);
  if (status != 0) {
    error = "DNS resolution failed with Winsock error " + std::to_string(status);
    return false;
  }
  for (addrinfo* current = result; current; current = current->ai_next) {
    char numeric[NI_MAXHOST]{};
    if (detail::winsock().getNameInfo(current->ai_addr,
                                      static_cast<socklen_t>(current->ai_addrlen),
                                      numeric, sizeof(numeric), nullptr, 0,
                                      NI_NUMERICHOST) == 0 &&
        std::find(addresses.begin(), addresses.end(), numeric) == addresses.end())
      addresses.emplace_back(numeric);
  }
  detail::winsock().freeAddressInfo(result);
  if (addresses.empty()) {
    error = "DNS resolution returned no TCP addresses";
    return false;
  }
  return true;
#else
  (void)host; (void)service; (void)addresses;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool connect(std::string_view host, std::int64_t port,
                    std::int64_t timeoutMilliseconds, Socket& connected,
                    std::string& error) {
#ifdef _WIN32
  connected = invalidSocket;
  if (host.empty() || port < 1 || port > 65535) {
    error = "TCP host must not be empty and port must be from 1 through 65535";
    return false;
  }
  if (!detail::validTimeout(timeoutMilliseconds, error) || !detail::ready(error))
    return false;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo* result = nullptr;
  const std::string hostText(host);
  const std::string service = std::to_string(port);
  const int resolved = detail::winsock().getAddressInfo(
      hostText.c_str(), service.c_str(), &hints, &result);
  if (resolved != 0) {
    error = "TCP DNS resolution failed with Winsock error " + std::to_string(resolved);
    return false;
  }
  const detail::Deadline deadline(timeoutMilliseconds);
  for (addrinfo* current = result; current; current = current->ai_next) {
    Socket socket = detail::winsock().createSocket(
        current->ai_family, current->ai_socktype, current->ai_protocol);
    if (socket == invalidSocket) continue;
    u_long nonblocking = 1;
    if (detail::winsock().controlSocket(socket, FIONBIO, &nonblocking) != 0) {
      detail::winsock().closeSocket(socket);
      continue;
    }
    int status = detail::winsock().connectSocket(
        socket, current->ai_addr, static_cast<int>(current->ai_addrlen));
    if (status != 0) {
      const int code = detail::winsock().lastError();
      if (code != WSAEWOULDBLOCK && code != WSAEINPROGRESS && code != WSAEINVAL) {
        detail::winsock().closeSocket(socket);
        continue;
      }
      std::string waitError;
      if (!detail::wait(socket, true, deadline.remainingMilliseconds(), waitError)) {
        detail::winsock().closeSocket(socket);
        error = std::move(waitError);
        continue;
      }
      int socketError = 0;
      int length = sizeof(socketError);
      if (detail::winsock().getSocketOption(
              socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError),
              &length) != 0 || socketError != 0) {
        detail::winsock().closeSocket(socket);
        error = "TCP connect failed with Winsock error " +
                std::to_string(socketError);
        continue;
      }
    }
    nonblocking = 0;
    detail::winsock().controlSocket(socket, FIONBIO, &nonblocking);
    connected = socket;
    break;
  }
  detail::winsock().freeAddressInfo(result);
  if (connected == invalidSocket) {
    if (error.empty()) error = "TCP connect failed for every resolved address";
    return false;
  }
  return true;
#else
  (void)host; (void)port; (void)timeoutMilliseconds; (void)connected;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool listen(std::string_view address, std::int64_t port,
                   std::int64_t backlog, Socket& listener, std::string& error) {
#ifdef _WIN32
  listener = invalidSocket;
  if (port < 0 || port > 65535 || backlog < 1 || backlog > SOMAXCONN) {
    error = "TCP listen port or backlog is outside its valid range";
    return false;
  }
  if (!detail::ready(error)) return false;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;
  addrinfo* result = nullptr;
  const std::string addressText(address);
  const std::string service = std::to_string(port);
  const int resolved = detail::winsock().getAddressInfo(
      address.empty() ? nullptr : addressText.c_str(), service.c_str(), &hints, &result);
  if (resolved != 0) {
    error = "TCP bind resolution failed with Winsock error " + std::to_string(resolved);
    return false;
  }
  for (addrinfo* current = result; current; current = current->ai_next) {
    Socket socket = detail::winsock().createSocket(
        current->ai_family, current->ai_socktype, current->ai_protocol);
    if (socket == invalidSocket) continue;
    int reuse = 1;
    detail::winsock().setSocketOption(socket, SOL_SOCKET, SO_REUSEADDR,
                                      reinterpret_cast<const char*>(&reuse),
                                      sizeof(reuse));
    if (detail::winsock().bindSocket(socket, current->ai_addr,
                                     static_cast<int>(current->ai_addrlen)) == 0 &&
        detail::winsock().listenSocket(socket, static_cast<int>(backlog)) == 0) {
      listener = socket;
      break;
    }
    detail::winsock().closeSocket(socket);
  }
  detail::winsock().freeAddressInfo(result);
  if (listener == invalidSocket) {
    error = detail::socketError("TCP listen");
    return false;
  }
  return true;
#else
  (void)address; (void)port; (void)backlog; (void)listener;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool accept(Socket listener, std::int64_t timeoutMilliseconds,
                   Socket& client, std::string& error) {
#ifdef _WIN32
  client = invalidSocket;
  if (!detail::validTimeout(timeoutMilliseconds, error) || !detail::ready(error) ||
      !detail::wait(listener, false, timeoutMilliseconds, error))
    return false;
  client = detail::winsock().acceptSocket(listener, nullptr, nullptr);
  if (client == invalidSocket) {
    error = detail::socketError("TCP accept");
    return false;
  }
  return true;
#else
  (void)listener; (void)timeoutMilliseconds; (void)client;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool send(Socket socket, std::string_view bytes,
                 std::int64_t timeoutMilliseconds, std::size_t& sent,
                 std::string& error) {
#ifdef _WIN32
  sent = 0;
  if (bytes.size() > 64 * 1024 * 1024) {
    error = "TCP send exceeds the 64 MiB limit";
    return false;
  }
  if (!detail::validTimeout(timeoutMilliseconds, error) || !detail::ready(error))
    return false;
  const detail::Deadline deadline(timeoutMilliseconds);
  while (sent < bytes.size()) {
    if (!detail::wait(socket, true, deadline.remainingMilliseconds(), error))
      return false;
    const int chunk = static_cast<int>((std::min)(
        bytes.size() - sent, static_cast<std::size_t>((std::numeric_limits<int>::max)())));
    const int written = detail::winsock().sendSocket(
        socket, bytes.data() + sent, chunk, 0);
    if (written <= 0) {
      error = detail::socketError("TCP send");
      return false;
    }
    sent += static_cast<std::size_t>(written);
  }
  return true;
#else
  (void)socket; (void)bytes; (void)timeoutMilliseconds; (void)sent;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool receive(Socket socket, std::size_t maximum,
                    std::int64_t timeoutMilliseconds, std::string& bytes,
                    std::string& error) {
#ifdef _WIN32
  if (maximum > 64 * 1024 * 1024) {
    error = "TCP receive exceeds the 64 MiB limit";
    return false;
  }
  if (!detail::validTimeout(timeoutMilliseconds, error) || !detail::ready(error))
    return false;
  if (maximum == 0) { bytes.clear(); return true; }
  if (!detail::wait(socket, false, timeoutMilliseconds, error)) return false;
  bytes.resize(maximum);
  const int chunk = static_cast<int>((std::min)(
      maximum, static_cast<std::size_t>((std::numeric_limits<int>::max)())));
  const int read = detail::winsock().receiveSocket(socket, bytes.data(), chunk, 0);
  if (read == SOCKET_ERROR) {
    bytes.clear();
    error = detail::socketError("TCP receive");
    return false;
  }
  bytes.resize(static_cast<std::size_t>(read));
  return true;
#else
  (void)socket; (void)maximum; (void)timeoutMilliseconds; (void)bytes;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool close(Socket socket, std::string& error) {
#ifdef _WIN32
  if (!detail::ready(error)) return false;
  if (detail::winsock().closeSocket(socket) != 0) {
    error = detail::socketError("TCP close");
    return false;
  }
  return true;
#else
  (void)socket;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool localPort(Socket socket, std::int64_t& port, std::string& error) {
#ifdef _WIN32
  if (!detail::ready(error)) return false;
  sockaddr_storage address{};
  int length = sizeof(address);
  if (detail::winsock().getSocketName(
          socket, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
    error = detail::socketError("TCP local endpoint query");
    return false;
  }
  const unsigned char* bytes = nullptr;
  if (address.ss_family == AF_INET)
    bytes = reinterpret_cast<const unsigned char*>(
        &reinterpret_cast<const sockaddr_in*>(&address)->sin_port);
  else if (address.ss_family == AF_INET6)
    bytes = reinterpret_cast<const unsigned char*>(
        &reinterpret_cast<const sockaddr_in6*>(&address)->sin6_port);
  else {
    error = "TCP local endpoint has an unsupported address family";
    return false;
  }
  port = static_cast<std::int64_t>((static_cast<unsigned>(bytes[0]) << 8) |
                                   static_cast<unsigned>(bytes[1]));
  return true;
#else
  (void)socket; (void)port;
  error = "networking is currently supported on Windows x64 only";
  return false;
#endif
}

inline bool httpRequest(std::string_view method, std::string_view url,
                        std::string_view body, std::int64_t timeoutMilliseconds,
                        HttpResponse& response, std::string& error) {
#ifdef _WIN32
  if (method.empty() || method.size() > 16 || body.size() > 64 * 1024 * 1024) {
    error = "HTTP method or body exceeds its documented limit";
    return false;
  }
  if (!detail::validTimeout(timeoutMilliseconds, error)) return false;
  const std::wstring wideUrl = detail::utf8ToWide(url);
  const std::wstring wideMethod = detail::utf8ToWide(method);
  if (wideUrl.empty() || wideMethod.empty()) {
    error = "HTTP method and URL must be valid non-empty UTF-8";
    return false;
  }
  detail::WinHttpApi api;
  if (!api.complete()) {
    error = "Windows WinHTTP is unavailable";
    return false;
  }
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = static_cast<DWORD>(-1);
  parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!api.crackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &parts) ||
      (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS)) {
    error = "HTTP URL must be an absolute http:// or https:// URL";
    return false;
  }
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring target(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength)
    target.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  if (target.empty()) target = L"/";
  HINTERNET session = api.open(L"Rocket/1.5", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) { error = detail::windowsError("WinHttpOpen"); return false; }
  HINTERNET connection = api.connect(session, host.c_str(), parts.nPort, 0);
  if (!connection) {
    error = detail::windowsError("WinHttpConnect");
    api.closeHandle(session);
    return false;
  }
  const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS
                          ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET request = api.openRequest(connection, wideMethod.c_str(), target.c_str(),
                                      nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!request) {
    error = detail::windowsError("WinHttpOpenRequest");
    api.closeHandle(connection); api.closeHandle(session);
    return false;
  }
  const int timeout = static_cast<int>(timeoutMilliseconds);
  bool success = api.setTimeouts(request, timeout, timeout, timeout, timeout) &&
                 api.sendRequest(
                     request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                     body.empty() ? WINHTTP_NO_REQUEST_DATA
                                  : const_cast<char*>(body.data()),
                     static_cast<DWORD>(body.size()),
                     static_cast<DWORD>(body.size()), 0) &&
                 api.receiveResponse(request, nullptr);
  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  if (success)
    success = api.queryHeaders(request,
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                               WINHTTP_NO_HEADER_INDEX);
  response.body.clear();
  while (success) {
    DWORD available = 0;
    if (!api.queryAvailable(request, &available)) { success = false; break; }
    if (available == 0) break;
    if (response.body.size() + available > 64 * 1024 * 1024) {
      error = "HTTP response exceeds the 64 MiB limit";
      success = false;
      break;
    }
    const std::size_t start = response.body.size();
    response.body.resize(start + available);
    DWORD read = 0;
    if (!api.readData(request, response.body.data() + start, available, &read)) {
      success = false;
      break;
    }
    response.body.resize(start + read);
  }
  if (!success && error.empty()) error = detail::windowsError("WinHTTP request");
  response.status = status;
  api.closeHandle(request); api.closeHandle(connection); api.closeHandle(session);
  return success;
#else
  (void)method; (void)url; (void)body; (void)timeoutMilliseconds; (void)response;
  error = "HTTP is currently supported on Windows x64 only";
  return false;
#endif
}

} // namespace rocket::platform_net
