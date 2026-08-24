#include "package_registry.h"

#include "package_docs.h"
#include "platform_credentials.h"
#include "platform_crypto.h"
#include "platform_net.h"
#include "safe_archive.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace rocket {
namespace {

constexpr std::uintmax_t maximumPackageBytes = 64ULL * 1024 * 1024;

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
  for (char& character : value)
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  return value;
}

bool validName(const std::string& value) {
  if (value.empty() ||
      (!std::isalpha(static_cast<unsigned char>(value.front())) &&
       value.front() != '_'))
    return false;
  return std::all_of(value.begin() + 1, value.end(), [](char character) {
    return std::isalnum(static_cast<unsigned char>(character)) ||
           character == '_' || character == '-';
  });
}

bool unquote(const std::string& text, std::string& value) {
  const std::string clean = trim(text);
  if (clean.size() < 2 || clean.front() != '"' || clean.back() != '"')
    return false;
  value.clear();
  bool escaped = false;
  for (std::size_t index = 1; index + 1 < clean.size(); ++index) {
    const char character = clean[index];
    if (escaped) {
      if (character == 'n') value.push_back('\n');
      else if (character == 't') value.push_back('\t');
      else if (character == 'r') value.push_back('\r');
      else if (character == '"' || character == '\\') value.push_back(character);
      else return false;
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      return false;
    } else {
      value.push_back(character);
    }
  }
  return !escaped;
}

std::string quote(const std::string& value) {
  std::string result = "\"";
  for (char character : value) {
    if (character == '\n') result += "\\n";
    else if (character == '\r') result += "\\r";
    else if (character == '\t') result += "\\t";
    else {
      if (character == '\\' || character == '"') result.push_back('\\');
      result.push_back(character);
    }
  }
  result.push_back('"');
  return result;
}

std::map<std::string, std::string> parseFields(const std::string& text) {
  std::map<std::string, std::string> fields;
  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#' || line.front() == '[') continue;
    const auto equal = line.find('=');
    std::string value;
    if (equal != std::string::npos && unquote(line.substr(equal + 1), value))
      fields[trim(line.substr(0, equal))] = std::move(value);
  }
  return fields;
}

bool readFile(const std::filesystem::path& path, std::string& bytes,
              std::string& error, std::uintmax_t maximum = maximumPackageBytes) {
  std::error_code filesystemError;
  const auto size = std::filesystem::file_size(path, filesystemError);
  if (filesystemError || size > maximum) {
    error = "registry file is unavailable or exceeds its documented limit";
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) { error = "could not read registry file '" + path.string() + "'"; return false; }
  bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  if (input.bad()) { error = "could not finish reading registry file"; return false; }
  return true;
}

bool writeFile(const std::filesystem::path& path, const std::string& bytes,
               std::string& error) {
  std::error_code filesystemError;
  std::filesystem::create_directories(path.parent_path(), filesystemError);
  if (filesystemError) {
    error = "could not create registry directory: " + filesystemError.message();
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) { error = "could not create registry file '" + path.string() + "'"; return false; }
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.flush();
  if (!output) { error = "could not write registry file '" + path.string() + "'"; return false; }
  return true;
}

bool transactionalWrite(const std::filesystem::path& path,
                        const std::string& bytes, std::string& error) {
#ifdef _WIN32
  const auto process = static_cast<unsigned long>(GetCurrentProcessId());
#else
  const auto process = static_cast<unsigned long>(::getpid());
#endif
  const auto partial = path.parent_path() /
                       (path.filename().string() + ".partial-" +
                        std::to_string(process));
  std::error_code filesystemError;
  std::filesystem::create_directories(path.parent_path(), filesystemError);
  if (filesystemError) {
    error = "could not create registry transaction directory";
    return false;
  }
  const std::string partialPrefix = path.filename().string() + ".partial-";
  for (const auto& item : std::filesystem::directory_iterator(path.parent_path(),
                                                               filesystemError)) {
    if (filesystemError) break;
    const std::string name = item.path().filename().string();
    if (!name.starts_with(partialPrefix)) continue;
    const auto status = item.symlink_status(filesystemError);
    if (filesystemError || std::filesystem::is_symlink(status) ||
        !std::filesystem::is_regular_file(status)) {
      error = "interrupted registry transaction is not a removable regular file";
      return false;
    }
    std::filesystem::remove(item.path(), filesystemError);
    if (filesystemError) {
      error = "could not recover an interrupted registry transaction";
      return false;
    }
  }
  if (filesystemError) {
    error = "could not inspect interrupted registry transactions";
    return false;
  }
  if (std::filesystem::exists(partial)) {
    const auto status = std::filesystem::symlink_status(partial, filesystemError);
    if (filesystemError || std::filesystem::is_symlink(status) ||
        !std::filesystem::is_regular_file(status)) {
      error = "interrupted registry transaction is not a removable regular file";
      return false;
    }
    std::filesystem::remove(partial, filesystemError);
    if (filesystemError) {
      error = "could not recover an interrupted registry transaction";
      return false;
    }
  }
  if (!writeFile(partial, bytes, error)) return false;
#ifdef _WIN32
  const bool moved = MoveFileExW(partial.c_str(), path.c_str(),
                                 MOVEFILE_REPLACE_EXISTING |
                                     MOVEFILE_WRITE_THROUGH) != 0;
  if (!moved) {
#else
  std::filesystem::rename(partial, path, filesystemError);
  if (filesystemError) {
#endif
    std::filesystem::remove(partial, filesystemError);
    error = "could not commit registry transaction";
    return false;
  }
  return true;
}

std::string canonicalRegistry(const std::filesystem::path& declaringRoot,
                              const std::string& registry,
                              std::filesystem::path& localRoot,
                              bool& remote, std::string& error) {
  remote = registry.starts_with("https://");
  if (remote) {
    std::string result = registry;
    while (!result.empty() && result.back() == '/') result.pop_back();
    return result;
  }
  if (registry.find("://") != std::string::npos &&
      !registry.starts_with("file://")) {
    error = "registry transport must use authenticated https:// or file://";
    return {};
  }
  std::string local = registry.starts_with("file://") ? registry.substr(7) : registry;
#ifdef _WIN32
  if (local.size() >= 3 && local.front() == '/' &&
      std::isalpha(static_cast<unsigned char>(local[1])) && local[2] == ':')
    local.erase(local.begin());
#endif
  localRoot = std::filesystem::path(local);
  if (localRoot.is_relative()) localRoot = declaringRoot / localRoot;
  localRoot = std::filesystem::absolute(localRoot).lexically_normal();
  return "file://" + localRoot.generic_string();
}

std::string httpsOrigin(const std::string& url) {
  if (!url.starts_with("https://")) return {};
  const auto slash = url.find('/', 8);
  return lower(slash == std::string::npos ? url : url.substr(0, slash));
}

bool httpsResource(const std::string& base, const std::string& resource,
                   std::string& bytes, std::string& error,
                   const std::vector<std::pair<std::string, std::string>>& headers = {}) {
  std::string url = base + resource;
  const std::string origin = httpsOrigin(url);
  if (origin.empty()) { error = "registry URL must use HTTPS"; return false; }
  for (int redirects = 0; redirects <= 3; ++redirects) {
    platform_net::HttpResponse response;
    if (!platform_net::httpRequestWithHeaders("GET", url, {}, headers, 30000,
                                              response, error))
      return false;
    if (response.status == 200) { bytes = std::move(response.body); return true; }
    if (response.status >= 300 && response.status < 400) {
      if (redirects == 3 || response.location.empty() ||
          httpsOrigin(response.location) != origin) {
        error = "registry redirect is missing, exceeds three hops, or changes HTTPS authority";
        return false;
      }
      url = response.location;
      continue;
    }
    error = "registry HTTPS request returned status " +
            std::to_string(response.status);
    return false;
  }
  error = "registry HTTPS redirect limit exceeded";
  return false;
}

bool registryResource(const std::filesystem::path& localRoot, bool remote,
                      const std::string& base, const std::string& resource,
                      std::string& bytes, std::string& error) {
  if (remote) return httpsResource(base, resource, bytes, error);
  std::string relative = resource;
  if (relative.starts_with("/v1/")) relative = relative.substr(4);
  else if (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
  return readFile(localRoot / std::filesystem::path(relative), bytes, error);
}

struct RegistryInfo {
  std::string canonical;
  std::filesystem::path localRoot;
  bool remote = false;
  std::string id;
  std::string publicKey;
  std::string fingerprint;
};

bool loadRegistry(const std::filesystem::path& declaringRoot,
                  const std::string& registry,
                  const std::string& expectedFingerprint,
                  RegistryInfo& info, std::string& error) {
  info.canonical = canonicalRegistry(declaringRoot, registry, info.localRoot,
                                     info.remote, error);
  if (!error.empty()) return false;
  std::string config;
  if (!registryResource(info.localRoot, info.remote, info.canonical,
                        "/v1/registry.toml", config, error))
    return false;
  const auto fields = parseFields(config);
  const auto version = fields.find("registry-version");
  const auto id = fields.find("id");
  const auto key = fields.find("public-key");
  if (version == fields.end() || version->second != "1" || id == fields.end() ||
      id->second.empty() || key == fields.end()) {
    error = "registry.toml is incomplete or has an unsupported version";
    return false;
  }
  info.id = id->second;
  info.publicKey = key->second;
  if (!platform_crypto::signingKeyFingerprint(info.publicKey, info.fingerprint,
                                               error))
    return false;
  if (expectedFingerprint.empty() || info.fingerprint != expectedFingerprint) {
    error = "registry signing-key fingerprint does not match package.registry-key";
    return false;
  }
  return true;
}

bool signedResource(const RegistryInfo& registry, const std::string& resource,
                    std::string& bytes, std::string& error) {
  std::string signature;
  if (!registryResource(registry.localRoot, registry.remote, registry.canonical,
                        resource, bytes, error) ||
      !registryResource(registry.localRoot, registry.remote, registry.canonical,
                        resource.substr(0, resource.size() - 5) + ".sig",
                        signature, error))
    return false;
  bool trusted = false;
  if (!platform_crypto::verifySignature(registry.publicKey, bytes,
                                         trim(signature), trusted, error))
    return false;
  if (!trusted) {
    error = "registry metadata signature verification failed";
    return false;
  }
  return true;
}

struct VersionRecord {
  std::string namespaceName;
  std::string version;
  std::string checksum;
  std::string archiveChecksum;
  std::string license;
  std::string publisher;
  bool yanked = false;
  std::string yankReason;
};

bool parseIndex(const std::string& text, const std::string& expectedName,
                std::vector<VersionRecord>& versions, std::string& error) {
  versions.clear();
  std::istringstream input(text);
  std::string line;
  std::map<std::string, std::string> header;
  std::map<std::string, std::string> current;
  auto finish = [&]() -> bool {
    if (current.empty()) return true;
    VersionRecord record;
    record.namespaceName = current["namespace"];
    record.version = current["version"];
    std::string checksum = current["checksum"];
    std::string archive = current["archive-checksum"];
    record.license = current["license"];
    record.publisher = current["publisher"];
    record.yanked = current["yanked"] == "true";
    record.yankReason = current["yank-reason"];
    if (checksum.starts_with("sha256:")) checksum = checksum.substr(7);
    if (archive.starts_with("sha256:")) archive = archive.substr(7);
    record.checksum = checksum;
    record.archiveChecksum = archive;
    if (!validName(record.namespaceName) ||
        !isValidSemanticVersion(record.version) ||
        record.checksum.size() != 64 || record.archiveChecksum.size() != 64 ||
        !isValidSpdxExpression(record.license) || record.publisher.empty()) {
      error = "signed registry index contains an incomplete version record";
      return false;
    }
    versions.push_back(std::move(record));
    current.clear();
    return true;
  };
  bool inVersion = false;
  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#') continue;
    if (line == "[[version]]") {
      if (!finish()) return false;
      inVersion = true;
      continue;
    }
    const auto equal = line.find('=');
    std::string value;
    if (equal == std::string::npos || !unquote(line.substr(equal + 1), value)) {
      error = "signed registry index contains invalid syntax";
      return false;
    }
    (inVersion ? current : header)[trim(line.substr(0, equal))] = value;
  }
  if (!finish()) return false;
  if (header["index-version"] != "1" || header["name"] != expectedName) {
    error = "signed registry index identity does not match the dependency";
    return false;
  }
  std::set<std::string> identities;
  for (const auto& version : versions)
    if (!identities.insert(version.namespaceName + "/" + version.version).second) {
      error = "signed registry index contains a duplicate version";
      return false;
    }
  return true;
}

std::string renderIndex(const std::string& name,
                        std::vector<VersionRecord> versions) {
  std::sort(versions.begin(), versions.end(), [](const auto& left, const auto& right) {
    const int compared = compareSemanticVersionText(left.version, right.version);
    if (compared != 0) return compared < 0;
    return left.namespaceName < right.namespaceName;
  });
  std::ostringstream output;
  output << "index-version = \"1\"\nname = " << quote(name) << "\n";
  for (const auto& version : versions) {
    output << "\n[[version]]\nnamespace = " << quote(version.namespaceName)
           << "\nversion = " << quote(version.version)
           << "\nchecksum = " << quote("sha256:" + version.checksum)
           << "\narchive-checksum = "
           << quote("sha256:" + version.archiveChecksum)
           << "\nlicense = " << quote(version.license)
           << "\npublisher = " << quote(version.publisher)
           << "\nyanked = " << quote(version.yanked ? "true" : "false")
           << "\nyank-reason = " << quote(version.yankReason) << "\n";
  }
  return output.str();
}

bool writeArchiveBytes(const std::filesystem::path& path,
                       const std::string& bytes, std::string& error) {
  return transactionalWrite(path, bytes, error);
}

bool extractArchive(const std::string& archiveBytes,
                    const std::filesystem::path& stagingRoot,
                    const std::string& expectedChecksum,
                    RegistrySelection& selection, std::string& error) {
  std::error_code filesystemError;
  std::filesystem::create_directories(stagingRoot, filesystemError);
  if (filesystemError) {
    error = "could not create package acquisition staging root";
    return false;
  }
#ifdef _WIN32
  const auto process = static_cast<unsigned long>(GetCurrentProcessId());
#else
  const auto process = static_cast<unsigned long>(::getpid());
#endif
  const auto archivePath = stagingRoot / ("package-" + std::to_string(process) + ".tar");
  const auto extracted = stagingRoot / ("source-" + std::to_string(process));
  if (std::filesystem::exists(extracted)) {
    const auto status = std::filesystem::symlink_status(extracted, filesystemError);
    if (filesystemError || std::filesystem::is_symlink(status) ||
        !std::filesystem::is_directory(status)) {
      error = "interrupted extraction staging path is unsafe";
      return false;
    }
    std::filesystem::remove_all(extracted, filesystemError);
    if (filesystemError) { error = "could not recover interrupted extraction"; return false; }
  }
  if (!writeArchiveBytes(archivePath, archiveBytes, error)) return false;
  std::vector<safe_archive::Entry> entries;
  if (!safe_archive::readAll(archivePath.generic_string(), entries, error)) {
    std::filesystem::remove(archivePath, filesystemError);
    return false;
  }
  std::filesystem::create_directories(extracted, filesystemError);
  if (filesystemError) { error = "could not create extraction transaction"; return false; }
  for (const auto& entry : entries) {
    const auto target = (extracted / std::filesystem::path(entry.name)).lexically_normal();
    const auto relative = target.lexically_relative(extracted);
    if (relative.empty() || *relative.begin() == "..") {
      error = "archive extraction path escaped its transaction root";
      std::filesystem::remove_all(extracted, filesystemError);
      return false;
    }
    std::filesystem::create_directories(target.parent_path(), filesystemError);
    if (filesystemError || !writeFile(target, entry.bytes, error)) {
      if (error.empty()) error = "could not extract package archive";
      std::filesystem::remove_all(extracted, filesystemError);
      return false;
    }
  }
  std::filesystem::remove(archivePath, filesystemError);
  std::string actual;
  if (!packageSourceChecksum(extracted, actual, error) || actual != expectedChecksum) {
    std::filesystem::remove_all(extracted, filesystemError);
    if (error.empty()) error = "signed registry package checksum mismatch";
    return false;
  }
  selection.sourceRoot = extracted;
  selection.temporaryRoot = extracted;
  return true;
}

bool acquireRecord(const RegistryInfo& registry, const std::string& name,
                   const VersionRecord& record,
                   const std::filesystem::path& stagingRoot,
                   RegistrySelection& selection, std::string& error) {
  const std::string resource = "/v1/packages/" + record.namespaceName + "/" +
                               name + "/" + record.version + ".tar";
  std::string archive;
  if (!registryResource(registry.localRoot, registry.remote, registry.canonical,
                        resource, archive, error))
    return false;
  std::string archiveChecksum;
  if (!platform_crypto::sha256(archive, archiveChecksum, error)) return false;
  if (archiveChecksum != record.archiveChecksum) {
    error = "registry archive checksum does not match signed metadata";
    return false;
  }
  selection = {};
  selection.namespaceName = record.namespaceName;
  selection.version = record.version;
  selection.checksum = record.checksum;
  selection.registryKey = registry.fingerprint;
  selection.publisher = record.publisher;
  selection.license = record.license;
  selection.yanked = record.yanked;
  selection.sourceIdentity = "registry:" + registry.canonical + "|" +
                             record.namespaceName + "/" + name + "@" +
                             record.version;
  return extractArchive(archive, stagingRoot, record.checksum, selection, error);
}

bool privateKey(const std::filesystem::path& root, std::string& key,
                std::string& error) {
  if (!readFile(root / "server/signing-private.key", key, error, 16 * 1024))
    return false;
  key = trim(key);
  return true;
}

bool signAndWrite(const std::filesystem::path& root,
                  const std::filesystem::path& relative,
                  const std::string& contents, std::string& error) {
  std::string key;
  std::string signature;
  if (!privateKey(root, key, error) ||
      !platform_crypto::sign(key, contents, signature, error))
    return false;
  const auto destination = root / relative;
  auto signaturePath = destination;
  signaturePath.replace_extension(".sig");
  return transactionalWrite(destination, contents, error) &&
         transactionalWrite(signaturePath, signature + "\n", error);
}

std::string registryIdentity(const std::string& registry,
                             std::filesystem::path& root,
                             bool& remote, std::string& error) {
  return canonicalRegistry(std::filesystem::current_path(), registry, root,
                           remote, error);
}

struct CredentialRecord {
  std::string id;
  std::string owner;
  std::set<std::string> scopes;
  std::string hash;
  bool revoked = false;
};

std::vector<std::string> split(const std::string& value, char separator = ';') {
  std::vector<std::string> result;
  std::size_t start = 0;
  while (start <= value.size()) {
    const auto end = value.find(separator, start);
    const std::string item = trim(value.substr(
        start, end == std::string::npos ? std::string::npos : end - start));
    if (!item.empty()) result.push_back(item);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return result;
}

bool tokenId(const std::string& token, std::string& id) {
  const auto dot = token.find('.');
  if (dot == std::string::npos) return false;
  id = token.substr(0, dot);
  return validName(id) && dot + 1 < token.size();
}

bool loadCredentialRecord(const std::filesystem::path& root,
                          const std::string& token, CredentialRecord& record,
                          std::string& error) {
  if (!tokenId(token, record.id)) {
    error = "registry token has an invalid credential identifier";
    return false;
  }
  std::string source;
  if (!readFile(root / "credentials" / (record.id + ".toml"), source, error,
                64 * 1024))
    return false;
  const auto fields = parseFields(source);
  if (!fields.contains("owner") || !fields.contains("scopes") ||
      !fields.contains("token-sha256")) {
    error = "registry credential record is incomplete";
    return false;
  }
  record.owner = fields.at("owner");
  record.hash = fields.at("token-sha256");
  record.revoked = fields.contains("revoked") && fields.at("revoked") == "true";
  for (const auto& scope : split(fields.at("scopes"))) record.scopes.insert(scope);
  std::string actual;
  if (!platform_crypto::sha256(token, actual, error)) return false;
  if (record.revoked || !platform_crypto::constantTimeEqual(actual, record.hash)) {
    error = record.revoked ? "registry credential is revoked"
                           : "registry credential is invalid";
    return false;
  }
  return true;
}

bool authorized(const std::string& registry, const std::string& scope,
                std::filesystem::path& root, CredentialRecord& record,
                std::string& error) {
  bool remote = false;
  const std::string canonical = registryIdentity(registry, root, remote, error);
  if (!error.empty()) return false;
  if (remote) {
    error = "this repository build requires an HTTPS registry service to enforce write authorization";
    return false;
  }
  std::string token;
  if (!platform_credentials::load(canonical, token, error)) return false;
  const bool valid = loadCredentialRecord(root, token, record, error);
  std::fill(token.begin(), token.end(), '\0');
  if (!valid) return false;
  if (!record.scopes.contains(scope) && !record.scopes.contains("admin")) {
    error = "stored registry credential lacks the required '" + scope + "' scope";
    return false;
  }
  return true;
}

bool loadNamespace(const std::filesystem::path& root,
                   const std::string& namespaceName,
                   std::map<std::string, std::string>& fields,
                   std::string& source, std::string& error) {
  if (!readFile(root / "namespaces" / (namespaceName + ".toml"), source, error,
                64 * 1024)) {
    error = "namespace ownership record is unavailable or exceeds its limit";
    return false;
  }
  std::string config;
  std::string signature;
  if (!readFile(root / "registry.toml", config, error, 64 * 1024) ||
      !readFile(root / "namespaces" / (namespaceName + ".sig"), signature,
                error, 16 * 1024))
    return false;
  const auto registryFields = parseFields(config);
  const auto key = registryFields.find("public-key");
  bool trusted = false;
  if (key == registryFields.end() ||
      !platform_crypto::verifySignature(key->second, source, trim(signature),
                                        trusted, error) ||
      !trusted) {
    if (error.empty()) error = "namespace ownership signature verification failed";
    return false;
  }
  fields = parseFields(source);
  if (fields["namespace"] != namespaceName || fields["owners"].empty()) {
    error = "namespace ownership record is incomplete";
    return false;
  }
  return true;
}

bool ownerMatches(const std::map<std::string, std::string>& fields,
                  const std::string& owner) {
  const auto found = fields.find("owners");
  if (found == fields.end()) return false;
  const auto owners = split(found->second);
  return std::find(owners.begin(), owners.end(), owner) != owners.end();
}

int editDistance(const std::string& leftValue, const std::string& rightValue) {
  const std::string left = lower(leftValue);
  const std::string right = lower(rightValue);
  std::vector<int> previous(right.size() + 1);
  for (std::size_t index = 0; index <= right.size(); ++index)
    previous[index] = static_cast<int>(index);
  for (std::size_t i = 1; i <= left.size(); ++i) {
    std::vector<int> current(right.size() + 1);
    current[0] = static_cast<int>(i);
    for (std::size_t j = 1; j <= right.size(); ++j)
      current[j] = (std::min)({current[j - 1] + 1, previous[j] + 1,
                               previous[j - 1] +
                                   (left[i - 1] == right[j - 1] ? 0 : 1)});
    previous = std::move(current);
  }
  return previous.back();
}

bool sourceEntries(const std::filesystem::path& root,
                   std::vector<safe_archive::Entry>& entries,
                   std::string& error) {
  entries.clear();
  std::error_code filesystemError;
  std::uintmax_t total = 0;
  std::filesystem::recursive_directory_iterator iterator(
      root, std::filesystem::directory_options::skip_permission_denied,
      filesystemError);
  const std::filesystem::recursive_directory_iterator end;
  while (iterator != end) {
    if (filesystemError) { error = "could not inspect publish source"; return false; }
    const auto status = iterator->symlink_status(filesystemError);
    if (filesystemError || std::filesystem::is_symlink(status)) {
      error = "publish source contains a symbolic link or unreadable entry";
      return false;
    }
    const std::string filename = iterator->path().filename().string();
    if (std::filesystem::is_directory(status) &&
        (filename == ".git" || filename == ".rocketc" || filename == "out" ||
         filename == "build" || filename == "dependencies" ||
         filename == "node_modules")) {
      iterator.disable_recursion_pending();
    } else if (std::filesystem::is_regular_file(status)) {
      const std::string name = iterator->path().lexically_relative(root).generic_string();
      if (!safe_archive::validEntryName(name)) {
        error = "publish source contains an unsafe archive path '" + name + "'";
        return false;
      }
      std::string bytes;
      if (!readFile(iterator->path(), bytes, error)) return false;
      total += bytes.size();
      if (total > maximumPackageBytes) {
        error = "publish source exceeds the 64 MiB content limit";
        return false;
      }
      entries.push_back({name, std::move(bytes)});
    }
    iterator.increment(filesystemError);
  }
  std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
    return left.name < right.name;
  });
  if (entries.empty() || entries.size() > 1024) {
    error = "publish source must contain 1 through 1024 files";
    return false;
  }
  return true;
}

} // namespace

bool isValidSpdxExpression(const std::string& expression) {
  static const std::unordered_set<std::string> licenses{
      "0BSD", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "BSL-1.0",
      "CC0-1.0", "GPL-2.0-only", "GPL-2.0-or-later", "GPL-3.0-only",
      "GPL-3.0-or-later", "ISC", "LGPL-2.1-only", "LGPL-2.1-or-later",
      "LGPL-3.0-only", "LGPL-3.0-or-later", "MIT", "MPL-2.0",
      "Unlicense", "Zlib"};
  static const std::unordered_set<std::string> exceptions{
      "Classpath-exception-2.0", "LLVM-exception", "OpenSSL-exception"};
  std::string spaced;
  for (char character : expression) {
    if (character == '(' || character == ')') {
      spaced.push_back(' '); spaced.push_back(character); spaced.push_back(' ');
    } else spaced.push_back(character);
  }
  std::istringstream input(spaced);
  std::vector<std::string> tokens{std::istream_iterator<std::string>(input), {}};
  if (tokens.empty()) return false;
  int depth = 0;
  bool expectLicense = true;
  bool afterWith = false;
  for (const auto& token : tokens) {
    if (expectLicense) {
      if (token == "(") { ++depth; continue; }
      if (afterWith ? !exceptions.contains(token) : !licenses.contains(token))
        return false;
      expectLicense = false;
      afterWith = false;
      continue;
    }
    if (token == ")") { if (--depth < 0) return false; continue; }
    if (token == "WITH") { afterWith = true; expectLicense = true; continue; }
    if (token == "AND" || token == "OR") { expectLicense = true; continue; }
    return false;
  }
  return !expectLicense && depth == 0;
}

bool acquireRegistryPackage(const std::filesystem::path& declaringRoot,
                            const std::string& registryLocation,
                            const std::string& expectedRegistryKey,
                            const std::string& name,
                            const std::string& requirement,
                            const std::filesystem::path& stagingRoot,
                            RegistrySelection& selection,
                            std::string& error) {
  RegistryInfo registry;
  if (!loadRegistry(declaringRoot, registryLocation, expectedRegistryKey,
                    registry, error))
    return false;
  std::string index;
  if (!signedResource(registry, "/v1/index/" + name + ".toml", index, error))
    return false;
  std::vector<VersionRecord> versions;
  if (!parseIndex(index, name, versions, error)) return false;
  std::vector<const VersionRecord*> candidates;
  for (const auto& version : versions)
    if (!version.yanked && semanticVersionSatisfies(version.version, requirement))
      candidates.push_back(&version);
  std::sort(candidates.begin(), candidates.end(), [](const auto* left, const auto* right) {
    return compareSemanticVersionText(left->version, right->version) > 0;
  });
  if (candidates.empty()) {
    error = "no non-yanked signed registry version of '" + name +
            "' satisfies '" + requirement + "'";
    return false;
  }
  return acquireRecord(registry, name, *candidates.front(), stagingRoot,
                       selection, error);
}

bool acquireLockedRegistryPackage(const std::filesystem::path& declaringRoot,
                                  const LockedPackage& locked,
                                  const std::filesystem::path& stagingRoot,
                                  RegistrySelection& selection,
                                  std::string& error) {
  if (!locked.source.starts_with("registry:")) {
    error = "locked package source is not a registry identity";
    return false;
  }
  const std::string source = locked.source.substr(9);
  const auto separator = source.rfind('|');
  if (separator == std::string::npos) {
    error = "locked registry identity is missing signed provenance";
    return false;
  }
  const std::string registryLocation = source.substr(0, separator);
  RegistryInfo registry;
  if (!loadRegistry(declaringRoot, registryLocation, locked.registryKey,
                    registry, error))
    return false;
  std::string index;
  if (!signedResource(registry, "/v1/index/" + locked.name + ".toml", index,
                      error))
    return false;
  std::vector<VersionRecord> versions;
  if (!parseIndex(index, locked.name, versions, error)) return false;
  for (const auto& record : versions) {
    if (record.namespaceName == locked.namespaceName &&
        record.version == locked.version) {
      if (record.checksum != locked.checksum ||
          record.publisher != locked.publisher) {
        error = "locked registry provenance differs from signed metadata";
        return false;
      }
      return acquireRecord(registry, locked.name, record, stagingRoot,
                           selection, error);
    }
  }
  error = "locked registry version is absent from signed metadata";
  return false;
}

void discardRegistrySelection(const RegistrySelection& selection) {
  if (selection.temporaryRoot.empty()) return;
  std::error_code ignored;
  std::filesystem::remove_all(selection.temporaryRoot, ignored);
}

bool auditLockedRegistryPackage(const std::filesystem::path& declaringRoot,
                                const LockedPackage& locked,
                                RegistryAuditStatus& status,
                                std::string& error) {
  status = {};
  if (!locked.source.starts_with("registry:")) {
    error = "audit expected a signed registry source";
    return false;
  }
  const std::string source = locked.source.substr(9);
  const auto separator = source.rfind('|');
  if (separator == std::string::npos) {
    error = "locked registry identity is missing signed provenance";
    return false;
  }
  RegistryInfo registry;
  if (!loadRegistry(declaringRoot, source.substr(0, separator),
                    locked.registryKey, registry, error))
    return false;

  std::string index;
  if (!signedResource(registry, "/v1/index/" + locked.name + ".toml",
                      index, error))
    return false;
  std::vector<VersionRecord> versions;
  if (!parseIndex(index, locked.name, versions, error)) return false;
  const VersionRecord* selected = nullptr;
  for (const auto& version : versions) {
    if (version.namespaceName == locked.namespaceName &&
        version.version == locked.version) {
      selected = &version;
      break;
    }
  }
  if (selected == nullptr || selected->checksum != locked.checksum ||
      selected->license != locked.license ||
      selected->publisher != locked.publisher) {
    error = "locked registry provenance differs from current signed metadata";
    return false;
  }
  status.provenance = registry.id + ":" + locked.namespaceName + "/" +
                      locked.name + "@" + locked.version +
                      " publisher=" + locked.publisher;
  status.yanked = selected->yanked;
  status.yankReason = selected->yankReason;

  std::string ownership;
  if (!signedResource(registry,
                      "/v1/namespaces/" + locked.namespaceName + ".toml",
                      ownership, error))
    return false;
  const auto ownershipFields = parseFields(ownership);
  if (ownershipFields.find("namespace") == ownershipFields.end() ||
      ownershipFields.at("namespace") != locked.namespaceName ||
      !ownershipFields.contains("transfer-history")) {
    error = "signed namespace ownership record is incomplete";
    return false;
  }

  std::string advisories;
  if (!signedResource(registry, "/v1/advisories.toml", advisories, error))
    return false;
  std::istringstream input(advisories);
  std::string line;
  std::map<std::string, std::string> current;
  auto finish = [&]() {
    if (current.empty()) return;
    const std::string identity = locked.namespaceName + "/" + locked.name;
    if (current["package"] == identity &&
        current["severity"] == "compromised" &&
        semanticVersionSatisfies(locked.version, current["affected"])) {
      std::string advisory = current["id"];
      if (!current["url"].empty()) advisory += " (" + current["url"] + ")";
      status.compromisedAdvisories.push_back(std::move(advisory));
    }
    current.clear();
  };
  while (std::getline(input, line)) {
    line = trim(line);
    if (line == "[[advisory]]") { finish(); continue; }
    const auto equal = line.find('=');
    std::string value;
    if (equal != std::string::npos &&
        unquote(line.substr(equal + 1), value))
      current[trim(line.substr(0, equal))] = std::move(value);
  }
  finish();
  std::sort(status.compromisedAdvisories.begin(),
            status.compromisedAdvisories.end());
  return true;
}

bool initializeReferenceRegistry(const std::filesystem::path& directory,
                                 const std::string& registryId,
                                 const std::string& owner,
                                 const std::string& token,
                                 std::string& fingerprint,
                                 std::string& error) {
  if (!validName(registryId) || !validName(owner)) {
    error = "registry ID and initial owner must be valid Rocket names";
    return false;
  }
  std::string id;
  if (!tokenId(token, id)) {
    error = "initial registry token must use <credential-id>.<secret>";
    return false;
  }
  const auto root = std::filesystem::absolute(directory).lexically_normal();
  if (std::filesystem::exists(root / "registry.toml")) {
    error = "registry is already initialized";
    return false;
  }
  platform_crypto::SigningKeyPair keys;
  if (!platform_crypto::generateSigningKeyPair(keys, error) ||
      !platform_crypto::signingKeyFingerprint(keys.publicKey, fingerprint,
                                               error))
    return false;
  const std::string config = "registry-version = \"1\"\nid = " + quote(registryId) +
                             "\npublic-key = " + quote(keys.publicKey) + "\n";
  std::string tokenHash;
  if (!platform_crypto::sha256(token, tokenHash, error)) return false;
  const std::string credential =
      "credential-version = \"1\"\nid = " + quote(id) + "\nowner = " +
      quote(owner) +
      "\nscopes = \"publish;yank;owner;security;admin\"\ntoken-sha256 = " +
      quote(tokenHash) + "\nrevoked = \"false\"\n";
  const std::string ownership =
      "namespace-version = \"1\"\nnamespace = " + quote(owner) +
      "\nowners = " + quote(owner) +
      "\ntransfer-history = \"created:" + owner + "\"\n";
  if (!writeFile(root / "registry.toml", config, error) ||
      !writeFile(root / "server/signing-private.key", keys.privateKey + "\n", error) ||
      !writeFile(root / "credentials" / (id + ".toml"), credential, error) ||
      !signAndWrite(root, std::filesystem::path("namespaces") /
                              (owner + ".toml"), ownership, error) ||
      !signAndWrite(root, "advisories.toml",
                    "advisory-version = \"1\"\n", error))
    return false;
  return true;
}

bool loginRegistry(const std::string& registry, const std::string& token,
                   std::string& error) {
  std::filesystem::path root;
  bool remote = false;
  const std::string canonical = registryIdentity(registry, root, remote, error);
  if (!error.empty()) return false;
  if (remote) {
    platform_net::HttpResponse response;
    if (!platform_net::httpRequestWithHeaders(
            "POST", canonical + "/v1/auth/verify", {},
            {{"Authorization", "Bearer " + token}}, 30000, response, error))
      return false;
    if (response.status != 204 && response.status != 200) {
      error = "registry rejected the supplied credential";
      return false;
    }
  } else {
    CredentialRecord record;
    if (!loadCredentialRecord(root, token, record, error)) return false;
  }
  return platform_credentials::store(canonical, token, error);
}

bool logoutRegistry(const std::string& registry, std::string& error) {
  std::filesystem::path root;
  bool remote = false;
  const std::string canonical = registryIdentity(registry, root, remote, error);
  if (!error.empty()) return false;
  return platform_credentials::erase(canonical, error);
}

bool publishPackage(const Package& package, std::string& report,
                    std::string& error) {
  if (package.registry.empty() || package.registryKey.empty()) {
    error = "publish requires package.registry and package.registry-key";
    return false;
  }
  if (!isValidSpdxExpression(package.license)) {
    error = "publish requires a valid SPDX license expression";
    return false;
  }
  if (!package.buildScript.empty()) {
    error = "package build scripts are not publishable in Rocket 1.6";
    return false;
  }
  for (const auto& dependency : package.dependencies)
    if (dependency.source != DependencySourceKind::Registry) {
      error = "published packages cannot contain path or Git dependencies";
      return false;
    }
  if (!package.dependencies.empty()) {
    PackageLock verifiedLock;
    std::vector<PackageDependencyRoot> verifiedRoots;
    if (!prepareLockedPackageDependencies(package, false, verifiedRoots,
                                          verifiedLock, error))
      return false;
  }
  RegistryInfo registry;
  if (!loadRegistry(package.root, package.registry, package.registryKey,
                    registry, error))
    return false;
  CredentialRecord credential;
  std::filesystem::path authorizedRoot;
  if (!registry.remote) {
    if (!authorized(package.registry, "publish", authorizedRoot, credential,
                    error))
      return false;
    std::map<std::string, std::string> ownership;
    std::string ownershipSource;
    if (!loadNamespace(registry.localRoot, package.namespaceName, ownership,
                       ownershipSource, error) ||
        !ownerMatches(ownership, credential.owner)) {
      if (error.empty()) error = "credential owner does not own the package namespace";
      return false;
    }
  }
  static const std::set<std::string> reserved{
      "rocket", "rocketc", "std", "standard", "security", "support"};
  if (reserved.contains(lower(package.name))) {
    error = "package name is reserved by registry policy";
    return false;
  }
  const auto indexDirectory = registry.localRoot / "index";
  std::error_code filesystemError;
  if (!registry.remote && std::filesystem::is_directory(indexDirectory)) {
    for (const auto& item : std::filesystem::directory_iterator(indexDirectory)) {
      if (item.path().extension() != ".toml") continue;
      const std::string existing = item.path().stem().string();
      if (lower(existing) == lower(package.name) && existing != package.name) {
        error = "registry package names cannot differ only by case";
        return false;
      }
      if (existing != package.name && editDistance(existing, package.name) <= 1) {
        error = "registry anti-typosquatting policy rejected package name '" +
                package.name + "' near existing package '" + existing + "'";
        return false;
      }
    }
  }
  std::vector<safe_archive::Entry> entries;
  if (!sourceEntries(package.root, entries, error)) return false;
  const auto staging = package.root / ".rocketc/publish";
  std::filesystem::create_directories(staging, filesystemError);
  if (filesystemError) { error = "could not create publish staging directory"; return false; }
  const auto archivePath = staging / "package.tar";
  std::string documentationReport;
  const auto documentationRoot = staging / "docs";
  std::filesystem::remove_all(documentationRoot, filesystemError);
  if (!generatePackageDocumentation(package, documentationRoot,
                                    documentationReport, error))
    return false;
  std::vector<safe_archive::Entry> documentationEntries;
  if (!sourceEntries(documentationRoot, documentationEntries, error))
    return false;
  if (!safe_archive::create(archivePath.generic_string(), entries, error)) return false;
  std::vector<safe_archive::Entry> verifiedEntries;
  if (!safe_archive::readAll(archivePath.generic_string(), verifiedEntries, error) ||
      verifiedEntries.size() != entries.size())
    return false;
  std::string archiveBytes;
  if (!readFile(archivePath, archiveBytes, error)) return false;
  std::string archiveChecksum;
  std::string sourceChecksum;
  if (!platform_crypto::sha256(archiveBytes, archiveChecksum, error) ||
      !packageSourceChecksum(package.root, sourceChecksum, error))
    return false;

  if (registry.remote) {
    std::vector<safe_archive::Entry> envelopeEntries{
        {"package.tar", archiveBytes}};
    for (const auto& item : documentationEntries)
      envelopeEntries.push_back({"docs/" + item.name, item.bytes});
    const auto envelopePath = staging / "publish-envelope.tar";
    if (!safe_archive::create(envelopePath.generic_string(), envelopeEntries,
                              error))
      return false;
    std::vector<safe_archive::Entry> verifiedEnvelope;
    if (!safe_archive::readAll(envelopePath.generic_string(), verifiedEnvelope,
                               error) ||
        verifiedEnvelope.size() != envelopeEntries.size())
      return false;
    std::string envelope;
    if (!readFile(envelopePath, envelope, error)) return false;
    std::string token;
    if (!platform_credentials::load(registry.canonical, token, error))
      return false;
    platform_net::HttpResponse response;
    const std::vector<std::pair<std::string, std::string>> headers{
        {"Authorization", "Bearer " + token},
        {"Content-Type", "application/vnd.rocket.publish.v1+tar"},
        {"Idempotency-Key", "sha256:" + archiveChecksum},
        {"X-Rocket-Namespace", package.namespaceName},
        {"X-Rocket-Name", package.name},
        {"X-Rocket-Version", package.version},
        {"X-Rocket-License", package.license},
        {"X-Rocket-Source-SHA256", sourceChecksum},
        {"X-Rocket-Archive-SHA256", archiveChecksum}};
    const bool requested = platform_net::httpRequestWithHeaders(
        "POST", registry.canonical + "/v1/packages", envelope, headers,
        30000, response, error);
    std::fill(token.begin(), token.end(), '\0');
    if (!requested) return false;
    if (response.status >= 300 && response.status < 400) {
      error = "registry HTTPS publish refused a redirect to protect credentials";
      return false;
    }
    if (response.status != 200 && response.status != 201) {
      error = "registry HTTPS publish returned status " +
              std::to_string(response.status);
      return false;
    }
    report = "published " + package.namespaceName + "/" + package.name + "@" +
             package.version + " sha256:" + sourceChecksum + "\n";
    return true;
  }

  const auto indexPath = registry.localRoot / "index" / (package.name + ".toml");
  std::vector<VersionRecord> versions;
  if (std::filesystem::is_regular_file(indexPath)) {
    std::string index;
    std::string signature;
    if (!readFile(indexPath, index, error) ||
        !readFile(indexPath.parent_path() / (package.name + ".sig"), signature,
                  error, 16 * 1024))
      return false;
    bool trusted = false;
    if (!platform_crypto::verifySignature(registry.publicKey, index,
                                           trim(signature), trusted, error) ||
        !trusted) {
      if (error.empty()) error = "existing registry index signature is invalid";
      return false;
    }
    if (!parseIndex(index, package.name, versions, error)) return false;
  }
  for (const auto& version : versions) {
    if (version.namespaceName == package.namespaceName &&
        version.version == package.version) {
      if (version.checksum == sourceChecksum &&
          version.archiveChecksum == archiveChecksum) {
        report = "publish retry verified existing immutable " +
                 package.namespaceName + "/" + package.name + "@" +
                 package.version + "\n";
        return true;
      }
      error = "registry version is immutable and already contains different bytes";
      return false;
    }
  }
  versions.push_back(VersionRecord{package.namespaceName, package.version,
                                   sourceChecksum, archiveChecksum,
                                   package.license, credential.owner, false, {}});
  const auto destination = registry.localRoot / "packages" /
                           package.namespaceName / package.name /
                           (package.version + ".tar");
  if (!transactionalWrite(destination, archiveBytes, error)) return false;
  for (const auto& item : documentationEntries) {
    if (!transactionalWrite(registry.localRoot / "docs" /
                                package.namespaceName / package.name /
                                package.version / item.name,
                            item.bytes, error))
      return false;
  }
  if (!signAndWrite(registry.localRoot,
                    std::filesystem::path("index") / (package.name + ".toml"),
                    renderIndex(package.name, versions), error))
    return false;
  std::ofstream audit(registry.localRoot / "audit.log", std::ios::binary | std::ios::app);
  audit << "publish " << package.namespaceName << '/' << package.name << '@'
        << package.version << " publisher=" << credential.owner
        << " sha256=" << sourceChecksum << '\n';
  report = "published " + package.namespaceName + "/" + package.name + "@" +
           package.version + " sha256:" + sourceChecksum + "\n";
  return true;
}

bool transferRegistryNamespace(const std::string& registry,
                               const std::string& namespaceName,
                               const std::string& newOwner,
                               std::string& error) {
  if (!validName(namespaceName) || !validName(newOwner)) {
    error = "namespace and new owner must be valid Rocket names";
    return false;
  }
  std::filesystem::path root;
  CredentialRecord credential;
  if (!authorized(registry, "owner", root, credential, error)) return false;
  std::map<std::string, std::string> fields;
  std::string source;
  if (!loadNamespace(root, namespaceName, fields, source, error) ||
      !ownerMatches(fields, credential.owner)) {
    if (error.empty()) error = "only a current namespace owner can transfer it";
    return false;
  }
  const std::string history = fields["transfer-history"] + ";" +
                              credential.owner + "->" + newOwner;
  const std::string updated =
      "namespace-version = \"1\"\nnamespace = " + quote(namespaceName) +
      "\nowners = " + quote(newOwner) + "\ntransfer-history = " +
      quote(history) + "\n";
  return signAndWrite(root, std::filesystem::path("namespaces") /
                                (namespaceName + ".toml"), updated, error);
}

bool yankRegistryPackage(const std::string& registry,
                         const std::string& identity,
                         const std::string& reason,
                         std::string& error) {
  if (reason.empty() || reason.size() > 512) {
    error = "yank reason must contain 1 through 512 bytes";
    return false;
  }
  const auto slash = identity.find('/');
  const auto at = identity.rfind('@');
  if (slash == std::string::npos || at == std::string::npos || slash >= at) {
    error = "yank identity must be namespace/name@version";
    return false;
  }
  const std::string namespaceName = identity.substr(0, slash);
  const std::string name = identity.substr(slash + 1, at - slash - 1);
  const std::string version = identity.substr(at + 1);
  std::filesystem::path root;
  CredentialRecord credential;
  if (!authorized(registry, "yank", root, credential, error)) return false;
  std::map<std::string, std::string> ownership;
  std::string ownershipSource;
  if (!loadNamespace(root, namespaceName, ownership, ownershipSource, error) ||
      !ownerMatches(ownership, credential.owner)) {
    if (error.empty()) error = "only a namespace owner can yank its package";
    return false;
  }
  std::string index;
  if (!readFile(root / "index" / (name + ".toml"), index, error)) return false;
  std::vector<VersionRecord> versions;
  if (!parseIndex(index, name, versions, error)) return false;
  bool found = false;
  for (auto& record : versions) {
    if (record.namespaceName == namespaceName && record.version == version) {
      record.yanked = true;
      record.yankReason = reason;
      found = true;
    }
  }
  if (!found) { error = "package version to yank does not exist"; return false; }
  return signAndWrite(root, std::filesystem::path("index") / (name + ".toml"),
                      renderIndex(name, versions), error);
}

bool revokeRegistryCredential(const std::string& registry,
                              const std::string& credentialId,
                              std::string& error) {
  if (!validName(credentialId)) { error = "credential ID is invalid"; return false; }
  std::filesystem::path root;
  CredentialRecord actor;
  if (!authorized(registry, "security", root, actor, error)) return false;
  std::string source;
  const auto path = root / "credentials" / (credentialId + ".toml");
  if (!readFile(path, source, error, 64 * 1024)) return false;
  auto fields = parseFields(source);
  if (fields["id"] != credentialId) { error = "credential record identity mismatch"; return false; }
  const std::string updated =
      "credential-version = \"1\"\nid = " + quote(credentialId) +
      "\nowner = " + quote(fields["owner"]) + "\nscopes = " +
      quote(fields["scopes"]) + "\ntoken-sha256 = " +
      quote(fields["token-sha256"]) + "\nrevoked = \"true\"\n";
  return transactionalWrite(path, updated, error);
}

bool publishRegistryAdvisory(const std::string& registry,
                             const std::filesystem::path& advisory,
                             std::string& error) {
  std::filesystem::path root;
  CredentialRecord actor;
  if (!authorized(registry, "security", root, actor, error)) return false;
  std::string source;
  if (!readFile(advisory, source, error, 64 * 1024)) return false;
  const auto fields = parseFields(source);
  if (!fields.contains("id") || !fields.contains("package") ||
      !fields.contains("affected") || fields.at("id").empty() ||
      fields.at("package").find('/') == std::string::npos ||
      fields.at("severity") != "compromised") {
    error = "advisory requires id, namespace/package, affected range, and severity=compromised";
    return false;
  }
  std::string existing;
  if (!readFile(root / "advisories.toml", existing, error, 1024 * 1024))
    return false;
  if (existing.find("id = " + quote(fields.at("id"))) != std::string::npos) {
    error = "advisory ID is immutable and already exists";
    return false;
  }
  std::ostringstream appended;
  appended << existing;
  if (!existing.empty() && existing.back() != '\n') appended << '\n';
  appended << "\n[[advisory]]\nid = " << quote(fields.at("id"))
           << "\npackage = " << quote(fields.at("package"))
           << "\naffected = " << quote(fields.at("affected"))
           << "\nseverity = \"compromised\"\nurl = "
           << quote(fields.contains("url") ? fields.at("url") : "") << "\n";
  return signAndWrite(root, "advisories.toml", appended.str(), error);
}

} // namespace rocket
