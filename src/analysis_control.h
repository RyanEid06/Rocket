#pragma once
#include "analysis_stop.h"
#include <filesystem>
#include <set>
#include <string>

namespace rocket {
struct AnalysisCancelled {};
// Opt-in per-thread compiler instrumentation. Ordinary CLI compilation has no
// active control. Cancellation unwinds the job's normal AST/HIR RAII owners.
struct AnalysisControl {
  StopToken stop;
  std::size_t reparsedFiles = 0;
  std::size_t semanticallyAnalyzedFiles = 0;
  std::set<std::string> invalidatedFiles;
  std::set<std::string> loadedFiles;
};
inline thread_local AnalysisControl *activeAnalysis = nullptr;
class AnalysisScope {
public:
  explicit AnalysisScope(AnalysisControl &control) : previous_(activeAnalysis) {
    activeAnalysis = &control;
  }
  ~AnalysisScope() { activeAnalysis = previous_; }
  AnalysisScope(const AnalysisScope &) = delete;
  AnalysisScope &operator=(const AnalysisScope &) = delete;

private:
  AnalysisControl *previous_;
};
inline void analysisCheckpoint() {
  if (activeAnalysis && activeAnalysis->stop.stop_requested())
    throw AnalysisCancelled{};
}
inline void recordAnalysisParse(const std::string &file) {
  analysisCheckpoint();
  if (activeAnalysis) {
    ++activeAnalysis->reparsedFiles;
    activeAnalysis->invalidatedFiles.insert(
        std::filesystem::path(file).lexically_normal().generic_string());
  }
}
} // namespace rocket
