#pragma once

#include "package.h"

#include <filesystem>
#include <string>

namespace rocket {

struct RegistrySelection {
  std::filesystem::path sourceRoot;
  std::filesystem::path temporaryRoot;
  std::string sourceIdentity;
  std::string namespaceName;
  std::string version;
  std::string checksum;
  std::string registryKey;
  std::string publisher;
  std::string license;
  bool yanked = false;
};

struct RegistryAuditStatus {
  std::string provenance;
  bool yanked = false;
  std::string yankReason;
  std::vector<std::string> compromisedAdvisories;
};

bool acquireRegistryPackage(const std::filesystem::path& declaringRoot,
                            const std::string& registry,
                            const std::string& expectedRegistryKey,
                            const std::string& name,
                            const std::string& requirement,
                            const std::filesystem::path& stagingRoot,
                            RegistrySelection& selection,
                            std::string& error);
bool acquireLockedRegistryPackage(const std::filesystem::path& declaringRoot,
                                  const LockedPackage& locked,
                                  const std::filesystem::path& stagingRoot,
                                  RegistrySelection& selection,
                                  std::string& error);
void discardRegistrySelection(const RegistrySelection& selection);
bool auditLockedRegistryPackage(const std::filesystem::path& declaringRoot,
                                const LockedPackage& locked,
                                RegistryAuditStatus& status,
                                std::string& error);

bool isValidSpdxExpression(const std::string& expression);
bool initializeReferenceRegistry(const std::filesystem::path& directory,
                                 const std::string& registryId,
                                 const std::string& owner,
                                 const std::string& token,
                                 std::string& fingerprint,
                                 std::string& error);
bool loginRegistry(const std::string& registry, const std::string& token,
                   std::string& error);
bool logoutRegistry(const std::string& registry, std::string& error);
bool publishPackage(const Package& package, std::string& report,
                    std::string& error);
bool transferRegistryNamespace(const std::string& registry,
                               const std::string& namespaceName,
                               const std::string& newOwner,
                               std::string& error);
bool yankRegistryPackage(const std::string& registry,
                         const std::string& identity,
                         const std::string& reason,
                         std::string& error);
bool revokeRegistryCredential(const std::string& registry,
                              const std::string& credentialId,
                              std::string& error);
bool publishRegistryAdvisory(const std::string& registry,
                             const std::filesystem::path& advisory,
                             std::string& error);

} // namespace rocket
