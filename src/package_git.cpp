#include "package_git.h"

#include "safe_archive.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace rocket {
namespace {

#ifdef _WIN32
std::wstring quoteArgument(const std::wstring& argument) {
  if (argument.empty()) return L"\"\"";
  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    return argument;
  std::wstring result = L"\"";
  std::size_t backslashes = 0;
  for (wchar_t character : argument) {
    if (character == L'\\') ++backslashes;
    else if (character == L'\"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(L'\"');
      backslashes = 0;
    } else {
      result.append(backslashes, L'\\');
      backslashes = 0;
      result.push_back(character);
    }
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}

bool runGit(const std::vector<std::string>& arguments,
            const std::filesystem::path& workingDirectory,
            const std::filesystem::path& capturePath, std::string& output,
            std::string& error) {
  std::vector<std::string> hardened{
      "-c", "core.hooksPath=NUL", "-c", "credential.helper=", "-c",
      "protocol.ext.allow=never", "-c", "protocol.file.allow=never", "-c",
      "submodule.recurse=false"};
  hardened.insert(hardened.end(), arguments.begin(), arguments.end());
  std::wstring commandLine = L"git.exe";
  for (const auto& argument : hardened)
    commandLine += L" " + quoteArgument(std::filesystem::path(argument).wstring());
  std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
  mutableCommand.push_back(L'\0');

  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE capture = CreateFileW(capturePath.c_str(), GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_DELETE, &security,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
  if (capture == INVALID_HANDLE_VALUE) {
    error = "could not create bounded Git output capture";
    return false;
  }
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = capture;
  startup.hStdError = capture;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION process{};
  const std::wstring directory = workingDirectory.wstring();
  const BOOL created = CreateProcessW(
      nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process);
  CloseHandle(capture);
  if (!created) {
    error = "could not start the reviewed git.exe transport";
    return false;
  }
  const DWORD waited = WaitForSingleObject(process.hProcess, 30000);
  if (waited == WAIT_TIMEOUT) {
    TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, 5000);
    error = "Git acquisition exceeded the 30 second operation limit";
  }
  DWORD exitCode = 1;
  GetExitCodeProcess(process.hProcess, &exitCode);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);

  std::error_code filesystemError;
  const auto size = std::filesystem::file_size(capturePath, filesystemError);
  if (filesystemError || size > 64ULL * 1024 * 1024) {
    error = "Git output exceeded the 64 MiB acquisition limit";
    return false;
  }
  std::ifstream input(capturePath, std::ios::binary);
  output.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  if (waited == WAIT_TIMEOUT) return false;
  if (exitCode != 0) {
    error = "Git transport failed without executing dependency code";
    return false;
  }
  return true;
}
#else
bool runGit(const std::vector<std::string>& arguments,
            const std::filesystem::path& workingDirectory,
            const std::filesystem::path& capturePath, std::string& output,
            std::string& error) {
  std::vector<std::string> hardened{
      "git", "-c", "core.hooksPath=/dev/null", "-c", "credential.helper=",
      "-c", "protocol.ext.allow=never", "-c", "protocol.file.allow=never",
      "-c", "submodule.recurse=false"};
  hardened.insert(hardened.end(), arguments.begin(), arguments.end());
  std::vector<char*> nativeArguments;
  nativeArguments.reserve(hardened.size() + 1);
  for (auto& argument : hardened) nativeArguments.push_back(argument.data());
  nativeArguments.push_back(nullptr);

  const int capture = ::open(capturePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                             S_IRUSR | S_IWUSR);
  if (capture < 0) {
    error = "could not create bounded Git output capture";
    return false;
  }

  const pid_t child = ::fork();
  if (child < 0) {
    ::close(capture);
    error = "could not start the reviewed git transport";
    return false;
  }
  if (child == 0) {
    ::setpgid(0, 0);
    if (::chdir(workingDirectory.c_str()) != 0 ||
        ::dup2(capture, STDOUT_FILENO) < 0 ||
        ::dup2(capture, STDERR_FILENO) < 0) {
      _exit(126);
    }
    ::close(capture);
    ::execvp("git", nativeArguments.data());
    _exit(errno == ENOENT ? 127 : 126);
  }
  ::close(capture);
  ::setpgid(child, child);

  int status = 0;
  bool completed = false;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(30);
  while (std::chrono::steady_clock::now() < deadline) {
    const pid_t waited = ::waitpid(child, &status, WNOHANG);
    if (waited == child) {
      completed = true;
      break;
    }
    if (waited < 0 && errno != EINTR) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (!completed) {
    ::kill(-child, SIGTERM);
    const auto terminateDeadline = std::chrono::steady_clock::now() +
                                   std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < terminateDeadline) {
      const pid_t waited = ::waitpid(child, &status, WNOHANG);
      if (waited == child) {
        completed = true;
        break;
      }
      if (waited < 0 && errno != EINTR) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!completed) {
      ::kill(-child, SIGKILL);
      while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    }
    error = "Git acquisition exceeded the 30 second operation limit";
  }

  std::error_code filesystemError;
  const auto size = std::filesystem::file_size(capturePath, filesystemError);
  if (filesystemError || size > 64ULL * 1024 * 1024) {
    error = "Git output exceeded the 64 MiB acquisition limit";
    return false;
  }
  std::ifstream input(capturePath, std::ios::binary);
  output.assign(std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
  if (!error.empty()) return false;
  if (!completed || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    error = "Git transport failed without executing dependency code";
    return false;
  }
  return true;
}
#endif

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
  for (char& character : value)
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  return value;
}

} // namespace

bool acquireGitPackage(const std::string& url, const std::string& revision,
                       const std::filesystem::path& stagingRoot,
                       GitAcquisition& acquisition, std::string& error) {
  acquisition = {};
  if (!url.starts_with("https://") || url.find('@', 8) != std::string::npos ||
      url.find('#') != std::string::npos || url.find('?') != std::string::npos) {
    error = "remote Git dependencies require a credential-free absolute HTTPS URL";
    return false;
  }
  if ((revision.size() != 40 && revision.size() != 64) ||
      !std::all_of(revision.begin(), revision.end(), [](char character) {
        return std::isxdigit(static_cast<unsigned char>(character));
      })) {
    error = "remote Git dependency revision is not an immutable object ID";
    return false;
  }
  std::error_code filesystemError;
  std::filesystem::create_directories(stagingRoot, filesystemError);
  if (filesystemError) { error = "could not create Git acquisition root"; return false; }
#ifdef _WIN32
  const auto process = static_cast<unsigned long>(GetCurrentProcessId());
#else
  const auto process = static_cast<unsigned long>(::getpid());
#endif
  const auto transaction = stagingRoot / ("git-" + std::to_string(process));
  if (std::filesystem::exists(transaction)) {
    const auto status = std::filesystem::symlink_status(transaction, filesystemError);
    if (filesystemError || std::filesystem::is_symlink(status) ||
        !std::filesystem::is_directory(status)) {
      error = "interrupted Git transaction path is unsafe";
      return false;
    }
    std::filesystem::remove_all(transaction, filesystemError);
    if (filesystemError) { error = "could not recover interrupted Git transaction"; return false; }
  }
  const auto repository = transaction / "repository.git";
  const auto extracted = transaction / "source";
  std::filesystem::create_directories(transaction, filesystemError);
  if (filesystemError) { error = "could not create Git transaction"; return false; }
  auto fail = [&]() {
    std::error_code ignored;
    std::filesystem::remove_all(transaction, ignored);
    return false;
  };
  std::string output;
  const auto capture = transaction / "git-output.bin";
  if (!runGit({"init", "--bare", repository.string()}, transaction, capture,
              output, error) ||
      !runGit({"--git-dir", repository.string(), "fetch", "--no-tags",
               "--depth=1", url, revision}, transaction, capture, output,
              error))
    return fail();
  if (!runGit({"--git-dir", repository.string(), "rev-parse",
               "FETCH_HEAD^{commit}"}, transaction, capture, output, error))
    return fail();
  if (lower(trim(output)) != lower(revision)) {
    error = "fetched Git object does not equal the requested immutable revision";
    return fail();
  }
  if (!runGit({"--git-dir", repository.string(), "ls-tree", "-r", "-z",
               "--full-tree", "FETCH_HEAD"}, transaction, capture, output,
              error))
    return fail();

  std::vector<std::pair<std::string, std::string>> objects;
  std::size_t offset = 0;
  while (offset < output.size()) {
    const auto end = output.find('\0', offset);
    if (end == std::string::npos) { error = "Git tree output is truncated"; return fail(); }
    const std::string record = output.substr(offset, end - offset);
    const auto tab = record.find('\t');
    const auto firstSpace = record.find(' ');
    const auto secondSpace = firstSpace == std::string::npos
                                 ? std::string::npos
                                 : record.find(' ', firstSpace + 1);
    if (tab == std::string::npos || firstSpace == std::string::npos ||
        secondSpace == std::string::npos) {
      error = "Git tree record is malformed";
      return fail();
    }
    const std::string mode = record.substr(0, firstSpace);
    const std::string type = record.substr(firstSpace + 1,
                                           secondSpace - firstSpace - 1);
    const std::string path = record.substr(tab + 1);
    if ((mode != "100644" && mode != "100755") || type != "blob" ||
        !safe_archive::validEntryName(path)) {
      error = "Git tree contains a submodule, link, non-regular mode, or unsafe path";
      return fail();
    }
    objects.push_back({path, "FETCH_HEAD:" + path});
    if (objects.size() > 1024) { error = "Git package contains more than 1024 files"; return fail(); }
    offset = end + 1;
  }
  std::vector<safe_archive::Entry> entries;
  std::uintmax_t total = 0;
  for (const auto& [path, object] : objects) {
    if (!runGit({"--git-dir", repository.string(), "show", object}, transaction,
                capture, output, error))
      return fail();
    total += output.size();
    if (total > 64ULL * 1024 * 1024) {
      error = "Git package exceeds the 64 MiB content limit";
      return fail();
    }
    entries.push_back({path, output});
  }
  const auto archive = transaction / "verified.tar";
  if (!safe_archive::create(archive.generic_string(), entries, error)) return fail();
  std::vector<safe_archive::Entry> verified;
  if (!safe_archive::readAll(archive.generic_string(), verified, error)) return fail();
  std::filesystem::create_directories(extracted, filesystemError);
  if (filesystemError) { error = "could not create verified Git export"; return fail(); }
  for (const auto& entry : verified) {
    const auto destination = extracted / std::filesystem::path(entry.name);
    std::filesystem::create_directories(destination.parent_path(), filesystemError);
    if (filesystemError) { error = "could not create Git export directory"; return fail(); }
    std::ofstream file(destination, std::ios::binary | std::ios::trunc);
    file.write(entry.bytes.data(), static_cast<std::streamsize>(entry.bytes.size()));
    if (!file) { error = "could not write verified Git export"; return fail(); }
  }
  acquisition.sourceRoot = extracted;
  acquisition.temporaryRoot = transaction;
  return true;
}

void discardGitAcquisition(const GitAcquisition& acquisition) {
  if (acquisition.temporaryRoot.empty()) return;
  std::error_code ignored;
  std::filesystem::remove_all(acquisition.temporaryRoot, ignored);
}

} // namespace rocket
