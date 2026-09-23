#include "analysis_control.h"
#include "analysis_queue.h"
#include "lexer.h"
#include "lsp_output_queue.h"
#include "parser.h"
#include "sema.h"
#include "test_support.h"
#include <condition_variable>
#include <future>
#include <sstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
int main() {
  int failures = 0;
  rocket::AnalysisQueue queue;
  std::mutex mutex;
  std::condition_variable cv;
  bool entered = false, release = false;
  std::vector<long long> published;
  auto first = std::async(std::launch::async, [&] {
    return queue.submit([&](std::stop_token, long long generation) {
      std::unique_lock lock(mutex);
      entered = true;
      cv.notify_all();
      cv.wait_for(lock, 2s, [&] { return release; });
      return [&, generation] { published.push_back(generation); };
    });
  });
  {
    std::unique_lock lock(mutex);
    rocket::test::expect(cv.wait_for(lock, 1s, [&] { return entered; }),
                         "old analysis entered controlled barrier", failures);
  }
  const bool responsive = first.wait_for(50ms) == std::future_status::ready;
  rocket::test::expect(
      responsive, "submit returns while older fake work is blocked", failures);
  // Keep the synchronous red scaffold race-free; the real queue exercises the
  // burst.
  if (responsive) {
    for (int i = 0; i < 1000; ++i)
      queue.submit([&](std::stop_token, long long generation) {
        return [&, generation] {
          published.push_back(generation);
          cv.notify_all();
        };
      });
    const auto status = queue.status();
    rocket::test::expect(status.generation == 1001 && status.started == 1 &&
                             status.pending && status.coalesced == 999,
                         "rapid edits retain exactly one pending newest input",
                         failures);
  }
  {
    std::lock_guard lock(mutex);
    release = true;
  }
  cv.notify_all();
  first.get();
  if (responsive) {
    for (int i = 0; i < 200 && queue.status().completed == 0; ++i)
      std::this_thread::sleep_for(5ms);
    queue.synchronize([&] {
      rocket::test::expect(published == std::vector<long long>{1001},
                           "stale work cannot publish after newer acceptance",
                           failures);
    });
    const auto status = queue.status();
    rocket::test::expect(
        status.started == 2 && status.completed == 1 && status.stale == 1 &&
            status.cancelled == 1,
        "queue telemetry counts actual starts, stale and cancelled work",
        failures);
  }
  rocket::test::expect(queue.shutdown(500ms),
                       "worker joins after draining newest work", failures);

  rocket::AnalysisQueue stopping;
  std::promise<void> running;
  auto stopped = std::async(std::launch::async, [&] {
    stopping.submit([&](std::stop_token stop, long long) {
      running.set_value();
      const auto deadline = std::chrono::steady_clock::now() + 2s;
      while (!stop.stop_requested() &&
             std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
      return rocket::AnalysisQueue::Publish{};
    });
  });
  running.get_future().wait();
  if (stopped.wait_for(50ms) == std::future_status::ready) {
    stopping.submit([](std::stop_token, long long) {
      return [] {
        throw std::runtime_error("queued work published during shutdown");
      };
    });
    const auto begin = std::chrono::steady_clock::now();
    rocket::test::expect(
        stopping.shutdown(500ms) &&
            std::chrono::steady_clock::now() - begin < 500ms,
        "shutdown cancels running and queued work and joins promptly",
        failures);
  } else {
    rocket::test::expect(false, "running fake does not block the caller",
                         failures);
  }
  stopped.get();
  // OS/foreign work that ignores cancellation must not retain or publish into
  // destroyed session owners. The fallback is observable as shutdown(false).
  std::promise<void> foreignEntered, foreignReleased;
  struct Completion {
    std::promise<void> done;
    ~Completion() { done.set_value(); }
  };
  auto completion = std::make_shared<Completion>();
  auto foreignFinished = completion->done.get_future();
  auto foreignRelease = foreignReleased.get_future().share();
  std::atomic<int> latePublications{0};
  {
    rocket::AnalysisQueue foreign;
    foreign.submit([&, completion](std::stop_token, long long) {
      foreignEntered.set_value();
      foreignRelease.wait();
      return [&, completion] { ++latePublications; };
    });
    completion.reset();
    foreignEntered.get_future().wait();
    const auto begin = std::chrono::steady_clock::now();
    rocket::test::expect(
        !foreign.shutdown(20ms) &&
            std::chrono::steady_clock::now() - begin < 500ms,
        "noncooperative foreign work reports bounded shutdown fallback",
        failures);
  }
  foreignReleased.set_value();
  foreignFinished.wait();
  rocket::test::expect(latePublications == 0,
                       "fallback worker cannot publish after owner destruction",
                       failures);
  rocket::Diagnostics diagnostics;
  const std::string source = "fn helper() -> Int:\n    return 0\n";
  auto tokens = rocket::Lexer("cancel.rocket", source, diagnostics).lex();
  auto module = rocket::Parser(tokens, diagnostics).parseModule();
  module.library = true;
  std::stop_source cancelled;
  cancelled.request_stop();
  rocket::AnalysisControl control{cancelled.get_token()};
  {
    rocket::AnalysisScope scope(control);
    int observed = 0;
    try {
      rocket::Lexer("cancel.rocket", source, diagnostics).lex();
    } catch (const rocket::AnalysisCancelled &) {
      ++observed;
    }
    try {
      rocket::Parser(tokens, diagnostics).parseModule();
    } catch (const rocket::AnalysisCancelled &) {
      ++observed;
    }
    try {
      rocket::SemanticAnalyzer(module, diagnostics).analyzeToHir();
    } catch (const rocket::AnalysisCancelled &) {
      ++observed;
    }
    rocket::test::expect(
        observed == 3,
        "real lexer/parser/semantic work observes job cancellation", failures);
  }
  rocket::test::expect(
      rocket::activeAnalysis == nullptr,
      "cancellation scope restores thread-local compiler state", failures);
  class SlowOutput : public std::stringbuf {
  public:
    std::promise<void> entered;
    std::shared_future<void> release;
    std::streamsize xsputn(const char *text, std::streamsize count) override {
      entered.set_value();
      release.wait();
      return std::stringbuf::xsputn(text, count);
    }
  } slowOutput;
  std::promise<void> releaseOutput;
  slowOutput.release = releaseOutput.get_future().share();
  std::ostream outputStream(&slowOutput);
  std::ostringstream outputLog;
  rocket::LspOutputQueue outputQueue(outputStream, outputLog, 32);
  outputQueue.send("frame");
  slowOutput.entered.get_future().wait();
  rocket::test::expect(
      outputQueue.send(std::string(32, 'x')) && !outputQueue.send("overflow") &&
          !outputQueue.send("after failure"),
      "blocked output has bounded nonblocking admission", failures);
  releaseOutput.set_value();
  rocket::test::expect(
      !outputQueue.shutdown() && slowOutput.str() == "frame",
      "transport overflow fails deterministically and joins borrowed stream",
      failures);
  return rocket::test::finish(failures, "analysis queue");
}
