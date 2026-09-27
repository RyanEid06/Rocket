#pragma once

#include "analysis_queue.h"
#include <iosfwd>
namespace rocket {

class LanguageServer {
public:
  LanguageServer(std::istream &input, std::ostream &output, std::ostream &log,
                 AnalysisQueue::Executor executor = {})
      : input_(input), output_(output), log_(log),
        executor_(std::move(executor)) {}

  int run();

private:
  std::istream& input_;
  std::ostream& output_;
  std::ostream& log_;
  AnalysisQueue::Executor executor_;
};

} // namespace rocket
