#include "lsp_session.h"
#include "test_support.h"
#include <filesystem>
#include <fstream>
int main() {
  int failures = 0;
  rocket::test::LspSession session;
  session.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  session.response("init");
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/count.rocket","version":1,"text":"fn helper() -> Int:\n    return 0\n"}}})");
  session.settle();
  session.send(
      R"({"jsonrpc":"2.0","id":"status","method":"rocket/projectStatus"})");
  const auto status = session.response("status");
  rocket::test::expect(
      status.find("\"reparsedFiles\":2") != std::string::npos &&
          status.find("\"semanticallyAnalyzedFiles\":1") != std::string::npos &&
          status.find("\"invalidatedFiles\":1") != std::string::npos,
      "telemetry counts source parse plus module parse and actual semantic "
      "file: " +
          status,
      failures);
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didSave","params":{"textDocument":{"uri":"file:///C:/workspace/count.rocket"}}})");
  session.settle();
  session.send(
      R"({"jsonrpc":"2.0","id":"saved","method":"rocket/projectStatus"})");
  const auto saved = session.response("saved");
  rocket::test::expect(saved.find("\"requestedGeneration\":1") !=
                           std::string::npos,
                       "no-text save preserves generation", failures);
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didSave","params":{"textDocument":{"uri":"file:///C:/workspace/count.rocket"},"text":"fn helper() -> Int:\n    return missing\n"}})");
  session.settle();
  session.send(
      R"({"jsonrpc":"2.0","id":"changedSave","method":"rocket/projectStatus"})");
  const auto changedSave = session.response("changedSave");
  rocket::test::expect(changedSave.find("\"requestedGeneration\":2") !=
                               std::string::npos &&
                           session.recorded.find("undefined name 'missing'") !=
                               std::string::npos,
                       "changed save enters same generation pipeline and "
                       "publishes semantic errors",
                       failures);
  session.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  session.response("shutdown");
  session.send(R"({"jsonrpc":"2.0","method":"exit"})");
  session.finish();
  // A completed old result is held behind a deterministic barrier. The real
  // protocol must still accept edits and serve status before it is released.
  std::promise<void> oldReady;
  std::promise<void> releaseOld;
  auto releaseFuture = releaseOld.get_future().share();
  rocket::test::LspSession responsive([&](rocket::AnalysisQueue::Work work,
                                          std::stop_token stop,
                                          long long generation) {
    auto publish = work(stop, generation);
    if (generation == 1) {
      oldReady.set_value();
      releaseFuture.wait_for(std::chrono::seconds(3));
    }
    return publish;
  });
  responsive.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  responsive.response("init");
  responsive.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/burst.rocket","version":1,"text":"fn helper() -> Int:\n    return missing\n"}}})");
  rocket::test::expect(
      oldReady.get_future().wait_for(std::chrono::seconds(2)) ==
          std::future_status::ready,
      "old real analysis reaches publication barrier", failures);
  responsive.send(
      R"({"jsonrpc":"2.0","id":"cancelledHover","method":"textDocument/hover","params":{"textDocument":{"uri":"file:///C:/workspace/burst.rocket"},"position":{"line":0,"character":4}}})");
  responsive.send(
      R"({"jsonrpc":"2.0","method":"$/cancelRequest","params":{"id":"cancelledHover"}})");
  responsive.send(
      R"({"jsonrpc":"2.0","id":"cancelBarrier","method":"rocket/projectStatus"})");
  responsive.response("cancelBarrier");
  rocket::test::expect(responsive.recorded.find("\"id\":\"cancelledHover\"") !=
                               std::string::npos &&
                           responsive.recorded.find("\"code\":-32800") !=
                               std::string::npos,
                       "queued request cancellation responds without waiting "
                       "for blocked analysis",
                       failures);
  for (int version = 2; version <= 101; ++version)
    responsive.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/burst.rocket","version":)" +
        std::to_string(version) +
        R"(},"contentChanges":[{"text":"fn helper() -> Int:\n    return 0\n"}]}})");
  responsive.send(
      R"({"jsonrpc":"2.0","id":"busy","method":"rocket/projectStatus"})");
  const auto busy = responsive.response("busy");
  rocket::test::expect(
      busy.find("\"requestedGeneration\":101") != std::string::npos &&
          busy.find("\"startedGenerations\":1") != std::string::npos &&
          busy.find("\"coalescedGenerations\":99") != std::string::npos &&
          busy.find("\"generation\":0") != std::string::npos,
      "protocol accepts newer edits with one pending slot while old analysis "
      "is blocked",
      failures);
  releaseOld.set_value();
  // Parse errors are emitted by the input loop while worker publications may
  // be emitted concurrently. OutputPipe validates every byte-counted frame.
  for (int i = 0; i < 100; ++i)
    responsive.send("{");
  responsive.settle();
  rocket::test::expect(
      responsive.recorded.find("undefined name 'missing'") ==
              std::string::npos &&
          responsive.recorded.find("\"version\":101") != std::string::npos &&
          responsive.recorded.find("\"version\":1,") == std::string::npos,
      "only newest captured text/version publishes diagnostics", failures);
  responsive.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  responsive.response("shutdown");
  responsive.send(R"({"jsonrpc":"2.0","method":"exit"})");
  responsive.finish();

  std::promise<void> running;
  std::atomic<bool> allowStop{false};
  rocket::test::LspSession stopping(
      [&](rocket::AnalysisQueue::Work, std::stop_token stop, long long) {
        running.set_value();
        while (!stop.stop_requested() || !allowStop.load())
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return rocket::AnalysisQueue::Publish{};
      });
  stopping.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  stopping.response("init");
  stopping.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/stop.rocket","version":1,"text":"fn helper() -> Int:\n    return 0\n"}}})");
  auto runningFuture = running.get_future();
  rocket::test::expect(runningFuture.wait_for(std::chrono::seconds(2)) ==
                           std::future_status::ready,
                       "shutdown fixture enters running fake analysis",
                       failures);
  // Hold the worker at the gate until both the queued edit and shutdown are
  // accepted; it must never execute/publish the queued generation.
  stopping.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/stop.rocket","version":2},"contentChanges":[{"text":"fn helper() -> Int:\n    return 1\n"}]}})");
  const auto stopStarted = std::chrono::steady_clock::now();
  stopping.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  stopping.response("shutdown");
  allowStop = true;
  stopping.send(R"({"jsonrpc":"2.0","method":"exit"})");
  rocket::test::expect(stopping.finish() == 0 &&
                           std::chrono::steady_clock::now() - stopStarted <
                               std::chrono::seconds(1) &&
                           stopping.recorded.find("publishDiagnostics") ==
                               std::string::npos,
                       "shutdown cancels queued/running fake protocol work and "
                       "joins within bound",
                       failures);

  rocket::test::LspSession graph;
  graph.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  graph.response("init");
  graph.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/math.rocket","version":1,"text":"pub fn doubled(value: Int) -> Int:\n    return value * 2\n"}}})");
  graph.settle();
  graph.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket","version":1,"text":"import math\nfn helper() -> Int:\n    return math.doubled(2)\n"}}})");
  graph.settle();
  graph.send(
      R"({"jsonrpc":"2.0","id":"graph","method":"rocket/projectStatus"})");
  const auto graphStatus = graph.response("graph");
  rocket::test::expect(
      graphStatus.find("\"reparsedFiles\":5") != std::string::npos &&
          graphStatus.find("\"semanticallyAnalyzedFiles\":3") !=
              std::string::npos &&
          graphStatus.find("\"invalidatedFiles\":2") != std::string::npos,
      "multi-root telemetry includes repeated imported module work, not root "
      "estimates",
      failures);
  graph.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  graph.response("shutdown");
  graph.send(R"({"jsonrpc":"2.0","method":"exit"})");
  graph.finish();

  std::promise<void> failReady, releaseFailure;
  auto failureRelease = releaseFailure.get_future().share();
  rocket::test::LspSession failing(
      [&](rocket::AnalysisQueue::Work work, std::stop_token stop,
          long long generation) -> rocket::AnalysisQueue::Publish {
        if (generation == 1) {
          failReady.set_value();
          failureRelease.wait_for(std::chrono::seconds(3));
          throw std::runtime_error("controlled analysis failure");
        }
        return work(stop, generation);
      });
  failing.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  failing.response("init");
  failing.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/failure.rocket","version":1,"text":"fn helper() -> Int:\n    return 0\n"}}})");
  failReady.get_future().wait();
  failing.send(
      R"({"jsonrpc":"2.0","id":"hover","method":"textDocument/hover","params":{"textDocument":{"uri":"file:///C:/workspace/failure.rocket"},"position":{"line":0,"character":4}}})");
  failing.send(
      R"({"jsonrpc":"2.0","id":"accepted","method":"rocket/projectStatus"})");
  failing.response("accepted");
  releaseFailure.set_value();
  failing.settle();
  // Ask a barrier after failure; a completed request must already be in
  // recorded.
  rocket::test::expect(
      failing.recorded.find("\"id\":\"hover\"") != std::string::npos &&
          failing.recorded.find("\"code\":-32603") != std::string::npos,
      "analysis failure completes queued requests with an error", failures);
  failing.send(
      R"({"jsonrpc":"2.0","id":"failureStatus","method":"rocket/projectStatus"})");
  const auto failureStatus = failing.response("failureStatus");
  rocket::test::expect(
      failureStatus.find("\"failedGenerations\":1") != std::string::npos &&
          failureStatus.find("\"completedGenerations\":0") !=
              std::string::npos &&
          failureStatus.find("\"generation\":0") != std::string::npos,
      "failed analysis is not reported as a completed snapshot", failures);
  failing.send(
      R"({"jsonrpc":"2.0","id":"lateHover","method":"textDocument/hover","params":{"textDocument":{"uri":"file:///C:/workspace/failure.rocket"},"position":{"line":0,"character":4}}})");
  const auto lateHover = failing.response("lateHover");
  rocket::test::expect(
      lateHover.find("\"code\":-32603") != std::string::npos,
      "requests after a failed generation cannot use an older snapshot",
      failures);
  failing.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  failing.response("shutdown");
  failing.send(R"({"jsonrpc":"2.0","method":"exit"})");
  failing.finish();

  // Discovery reuse must be visible in real file inventory, not just counters.
  const auto fixture =
      std::filesystem::current_path() /
      ("lsp-analysis-discovery-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(fixture);
  {
    std::ofstream file(fixture / "first.rocket");
    file << "fn first() -> Int:\n    return 1\n";
  }
  auto rootUri = std::string("file:///") + fixture.generic_string();
#ifndef _WIN32
  rootUri = "file://" + fixture.generic_string();
#endif
  rocket::test::LspSession discovery;
  discovery.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{"rootUri":")" +
      rootUri + R"("}})");
  discovery.response("init");
  discovery.send(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
  discovery.settle();
  {
    std::ofstream file(fixture / "second.rocket");
    file << "fn second() -> Int:\n    return 2\n";
  }
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":")" +
      rootUri + R"(/first.rocket","type":2}]}})");
  discovery.settle();
  discovery.send(
      R"({"jsonrpc":"2.0","id":"ordinary","method":"rocket/projectStatus"})");
  auto ordinary = discovery.response("ordinary");
  rocket::test::expect(
      ordinary.find("\"files\":1") != std::string::npos &&
          ordinary.find("\"discoveryCacheMisses\":1") != std::string::npos &&
          ordinary.find("\"discoveryCacheHits\":1") != std::string::npos,
      "watched source modification reuses inventory", failures);
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":")" +
      rootUri + R"(/second.rocket","type":1}]}})");
  discovery.settle();
  discovery.send(
      R"({"jsonrpc":"2.0","id":"created","method":"rocket/projectStatus"})");
  const auto created = discovery.response("created");
  rocket::test::expect(
      created.find("\"files\":2") != std::string::npos &&
          created.find("\"discoveryCacheMisses\":2") != std::string::npos,
      "watched creation invalidates discovery and sees new source", failures);
  auto discoveryStatus = [&](const std::string &id) {
    discovery.settle();
    discovery.send(R"({"jsonrpc":"2.0","id":")" + id +
                   R"(","method":"rocket/projectStatus"})");
    return discovery.response(id);
  };
  std::filesystem::remove(fixture / "second.rocket");
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":")" +
      rootUri + R"(/second.rocket","type":3}]}})");
  auto deleted = discoveryStatus("deleted");
  rocket::test::expect(deleted.find("\"discoveryCacheMisses\":3") !=
                               std::string::npos &&
                           deleted.find("\"files\":1") != std::string::npos,
                       "watched deletion invalidates inventory", failures);
  std::filesystem::rename(fixture / "first.rocket", fixture / "renamed.rocket");
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didRenameFiles","params":{"files":[{"oldUri":")" +
      rootUri + R"(/first.rocket","newUri":")" + rootUri +
      R"(/renamed.rocket"}]}})");
  auto renamed = discoveryStatus("renamed");
  rocket::test::expect(renamed.find("\"discoveryCacheMisses\":4") !=
                               std::string::npos &&
                           renamed.find("\"files\":1") != std::string::npos,
                       "explicit file rename invalidates inventory", failures);
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":")" +
      rootUri + R"(/rocket.toml","type":2}]}})");
  auto manifest = discoveryStatus("manifest");
  rocket::test::expect(
      manifest.find("\"discoveryCacheMisses\":5") != std::string::npos,
      "manifest modification invalidates discovery and dependency roots",
      failures);
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeConfiguration","params":{"settings":{"rocket":{"languageServer":{"maximumProjectFiles":32,"toolchain":"updated"}}}}})");
  auto configured = discoveryStatus("configured");
  rocket::test::expect(
      configured.find("\"discoveryCacheMisses\":6") != std::string::npos &&
          configured.find("\"maximumProjectFiles\":32") != std::string::npos,
      "configuration and toolchain-change notification invalidates discovery",
      failures);
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":")" +
      rootUri + R"(/notes.txt","type":1},{"uri":")" + rootUri +
      R"(/renamed.rocket","type":9}]}})");
  auto irrelevant = discoveryStatus("irrelevant");
  rocket::test::expect(irrelevant.find("\"discoveryCacheMisses\":6") !=
                           std::string::npos,
                       "irrelevant files and invalid watched event types do "
                       "not invalidate discovery",
                       failures);
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWorkspaceFolders","params":{"event":{"added":[],"removed":[{"uri":")" +
      rootUri + R"(","name":"old"}]}}})");
  auto removed = discoveryStatus("removed");
  rocket::test::expect(
      removed.find("\"discoveryCacheMisses\":7") != std::string::npos &&
          removed.find("\"files\":0") != std::string::npos,
      "folder removal captures new workspace and clears inventory", failures);
  discovery.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWorkspaceFolders","params":{"event":{"added":[{"uri":")" +
      rootUri + R"(","name":"new"}],"removed":[]}}})");
  auto added = discoveryStatus("added");
  rocket::test::expect(
      added.find("\"discoveryCacheMisses\":8") != std::string::npos &&
          added.find("\"files\":1") != std::string::npos,
      "folder addition captures dependency roots and rediscovers sources",
      failures);
  discovery.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
      rootUri +
      R"(/rocket.toml","version":1,"text":"[package]\nname = \"sample\"\nversion = \"0.1.0\"\nentry = \"renamed.rocket\"\n"}}})");
  discoveryStatus("manifestOpen");
  {
    std::ofstream file(fixture / "rocket.toml");
    file << "[package]\nname = \"sample\"\nversion = \"0.1.0\"\nentry = "
            "\"renamed.rocket\"\n";
  }
  discovery.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didSave","params":{"textDocument":{"uri":")" +
      rootUri + R"(/rocket.toml"}}})");
  auto manifestSaved = discoveryStatus("manifestSaved");
  rocket::test::expect(
      manifestSaved.find("\"discoveryCacheMisses\":10") != std::string::npos &&
          manifestSaved.find("\"semanticallyAnalyzedFiles\":1") !=
              std::string::npos,
      "manifest save reloads newly persisted package roots even when buffer "
      "text is unchanged",
      failures);
  discovery.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  discovery.response("shutdown");
  discovery.send(R"({"jsonrpc":"2.0","method":"exit"})");
  discovery.finish();
  std::filesystem::remove_all(fixture);
  // Race a queued request batch with edits/shutdown immediately after commit.
  // Each admitted ID must complete exactly once before the control barrier;
  // requests cannot disappear into callbacks or run against a newer document.
  for (const bool closeBatch : {false, true}) {
    std::promise<void> batchReady, releaseBatch, batchPublished;
    auto batchRelease = releaseBatch.get_future().share();
    auto committed = batchPublished.get_future();
    rocket::test::LspSession batch([&](rocket::AnalysisQueue::Work work,
                                       std::stop_token stop,
                                       long long generation) {
      if (generation == 1) {
        auto publish = work(stop, generation);
        batchReady.set_value();
        batchRelease.wait();
        return rocket::AnalysisQueue::Publish(
            [&, publish = std::move(publish)] {
              publish();
              batchPublished.set_value();
            });
      }
      while (!stop.stop_requested())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      return rocket::AnalysisQueue::Publish{};
    });
    batch.send(
        R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
    batch.response("init");
    batch.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/batch.rocket","version":1,"text":"fn helper() -> Int:\n    return 0\n"}}})");
    batchReady.get_future().wait();
    for (int i = 0; i < 256; ++i)
      batch.send(
          R"({"jsonrpc":"2.0","id":"batch)" + std::to_string(i) +
          R"(","method":"textDocument/hover","params":{"textDocument":{"uri":"file:///C:/workspace/batch.rocket"},"position":{"line":0,"character":4}}})");
    batch.send(
        R"({"jsonrpc":"2.0","id":"queued","method":"rocket/projectStatus"})");
    batch.response("queued");
    releaseBatch.set_value();
    committed.wait();
    if (closeBatch) {
      batch.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
      batch.response("shutdown");
    } else {
      batch.send(
          R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/batch.rocket","version":2},"contentChanges":[{"text":""}]}})");
      batch.send(
          R"({"jsonrpc":"2.0","id":"changed","method":"rocket/projectStatus"})");
      batch.response("changed");
    }
    for (int i = 0; i < 256; ++i) {
      const auto id = "\"id\":\"batch" + std::to_string(i) + "\"";
      const auto first = batch.recorded.find(id);
      rocket::test::expect(
          first != std::string::npos &&
              batch.recorded.find(id, first + 1) == std::string::npos,
          "posted request completes exactly once at edit/shutdown barrier: " +
              id,
          failures);
    }
    if (!closeBatch) {
      batch.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
      batch.response("shutdown");
    }
    batch.send(R"({"jsonrpc":"2.0","method":"exit"})");
    batch.finish();
  }
  // A backed-up client must not retain the acceptance/publication gate.
  class BlockedOutput : public rocket::test::OutputPipe {
  public:
    void block() {
      std::lock_guard lock(mutex);
      blocking = true;
    }
    void release() {
      std::lock_guard lock(mutex);
      blocking = false;
      cv.notify_all();
    }
    std::promise<void> entered;

  protected:
    std::streamsize xsputn(const char *text, std::streamsize count) override {
      std::unique_lock lock(mutex);
      if (blocking) {
        entered.set_value();
        cv.wait(lock, [&] { return !blocking; });
      }
      lock.unlock();
      return OutputPipe::xsputn(text, count);
    }

  private:
    std::mutex mutex;
    std::condition_variable cv;
    bool blocking = false;
  } blockedOutput;
  rocket::test::InputPipe blockedInput;
  std::istream blockedInputStream(&blockedInput);
  std::ostream blockedOutputStream(&blockedOutput);
  std::ostringstream blockedLog;
  std::promise<void> acceptedWithBlockedOutput, cancelledWithBlockedOutput;
  auto acceptedOutputFuture = acceptedWithBlockedOutput.get_future();
  auto cancelledOutputFuture = cancelledWithBlockedOutput.get_future();
  auto blockedServer = std::async(std::launch::async, [&] {
    rocket::LanguageServer server(
        blockedInputStream, blockedOutputStream, blockedLog,
        [&](rocket::AnalysisQueue::Work work, std::stop_token stop,
            long long generation) {
          if (generation == 2) {
            acceptedWithBlockedOutput.set_value();
            while (!stop.stop_requested())
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cancelledWithBlockedOutput.set_value();
            return rocket::AnalysisQueue::Publish{};
          }
          return work(stop, generation);
        });
    return server.run();
  });
  auto blockedSend = [&](const std::string &body) {
    blockedInput.put(rocket::test::lspFrame(body));
  };
  blockedSend(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  blockedOutput.next();
  blockedOutput.block();
  blockedSend(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/backpressure.rocket","version":1,"text":"fn helper() -> Int:\n    return 0\n"}}})");
  rocket::test::expect(
      blockedOutput.entered.get_future().wait_for(std::chrono::seconds(2)) ==
          std::future_status::ready,
      "publication reaches blocked transport", failures);
  blockedSend(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/backpressure.rocket","version":2},"contentChanges":[{"text":"fn helper() -> Int:\n    return 1\n"}]}})");
  rocket::test::expect(
      acceptedOutputFuture.wait_for(std::chrono::seconds(1)) ==
          std::future_status::ready,
      "new input accepted while diagnostics transport is blocked", failures);
  blockedSend(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  blockedSend(R"({"jsonrpc":"2.0","method":"exit"})");
  rocket::test::expect(
      cancelledOutputFuture.wait_for(std::chrono::seconds(1)) ==
          std::future_status::ready,
      "analysis cancels while diagnostics transport is blocked", failures);
  blockedOutput.release();
  blockedInput.close();
  rocket::test::expect(blockedServer.get() == 0,
                       "transport drains after release", failures);
  return rocket::test::finish(failures, "language server analysis");
}
