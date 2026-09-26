#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

namespace rocket {

// The gate linearizes protocol acceptance and publication. Work must own its
// inputs and must not touch the session; only Publish may access session state.
class AnalysisQueue {
public:
  using Publish = std::function<void()>;
  using Work = std::function<Publish(std::stop_token, long long)>;
  using Executor = std::function<Publish(Work, std::stop_token, long long)>;
  using Failure = std::function<void(std::exception_ptr)>;
  struct Status {
    long long generation = 0, started = 0, completed = 0, stale = 0;
    long long cancelled = 0, coalesced = 0, failed = 0;
    bool running = false, pending = false;
  };

  explicit AnalysisQueue(Executor executor = {}, Failure failure = {})
      : state_(
            std::make_shared<State>(std::move(executor), std::move(failure))),
        worker_([state = state_] { run(state); }) {}
  ~AnalysisQueue() { shutdown(); }
  AnalysisQueue(const AnalysisQueue &) = delete;
  AnalysisQueue &operator=(const AnalysisQueue &) = delete;

  template <class F> void synchronize(F &&action) {
    std::lock_guard lock(state_->mutex);
    action();
  }

  long long submit(Work work) {
    std::lock_guard lock(state_->mutex);
    if (state_->stopping)
      return state_->status.generation;
    if (state_->job)
      ++state_->status.coalesced;
    state_->stop.request_stop();
    const auto generation = ++state_->status.generation;
    state_->job = Job{generation, std::move(work)};
    state_->status.pending = true;
    state_->cv.notify_all();
    return generation;
  }

  // Small session callbacks execute individually under the same lifetime gate.
  // Releasing it between requests prevents a whole batch delaying acceptance.
  bool post(Publish action) {
    std::lock_guard lock(state_->mutex);
    if (state_->stopping || state_->callbacks.size() >= 256)
      return false;
    state_->callbacks.push_back(std::move(action));
    state_->cv.notify_all();
    return true;
  }

  Status status() const {
    std::lock_guard lock(state_->mutex);
    return state_->status;
  }

  void cancel() {
    std::lock_guard lock(state_->mutex);
    state_->stopping = true;
    state_->callbacks.clear();
    state_->stop.request_stop();
    if (state_->job) {
      ++state_->status.cancelled;
      state_->job.reset();
    }
    state_->status.pending = false;
    state_->cv.notify_all();
  }

  // Cooperative compiler work normally joins immediately. If an OS file read or
  // foreign executor cannot return by the deadline, the isolated shared state
  // outlives the session. No publication can run after cancel(). Never destroy
  // or force-terminate a thread that still owns compiler state.
  bool shutdown(std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    cancel();
    if (!worker_.joinable())
      return joined_;
    std::unique_lock lock(state_->mutex);
    joined_ = state_->cv.wait_for(lock, timeout, [&] { return state_->done; });
    lock.unlock();
    if (joined_)
      worker_.join();
    else
      worker_.detach();
    return joined_;
  }

private:
  struct Job {
    long long generation;
    Work work;
  };
  struct State {
    explicit State(Executor run, Failure fail)
        : executor(std::move(run)), failure(std::move(fail)) {}
    Executor executor;
    Failure failure;
    std::recursive_mutex mutex;
    std::condition_variable_any cv;
    std::optional<Job> job;
    std::deque<Publish> callbacks;
    std::stop_source stop;
    Status status;
    bool stopping = false, done = false;
  };

  static void run(const std::shared_ptr<State> &state) {
    std::unique_lock lock(state->mutex);
    while (!state->stopping) {
      state->cv.wait(lock, [&] {
        return state->stopping || state->job.has_value() ||
               !state->callbacks.empty();
      });
      if (state->stopping)
        break;
      if (!state->callbacks.empty()) {
        auto callback = std::move(state->callbacks.front());
        state->callbacks.pop_front();
        try {
          callback();
        } catch (...) {
          try {
            if (state->failure)
              state->failure(std::current_exception());
          } catch (...) {
          }
        }
        lock.unlock();
        std::this_thread::yield();
        lock.lock();
        continue;
      }
      Job job = std::move(*state->job);
      state->job.reset();
      state->stop = std::stop_source{};
      const auto stop = state->stop.get_token();
      state->status.pending = false;
      state->status.running = true;
      ++state->status.started;
      lock.unlock();
      Publish publish;
      std::exception_ptr failure;
      try {
        publish = state->executor ? state->executor(std::move(job.work), stop,
                                                    job.generation)
                                  : job.work(stop, job.generation);
      } catch (...) {
        if (!stop.stop_requested())
          failure = std::current_exception();
      }
      lock.lock();
      state->status.running = false;
      if (failure)
        ++state->status.failed;
      if (stop.stop_requested())
        ++state->status.cancelled;
      if (job.generation != state->status.generation)
        ++state->status.stale;
      else if (!state->stopping && !stop.stop_requested() && failure) {
        try {
          if (state->failure)
            state->failure(failure);
        } catch (...) { /* Failure reporting cannot terminate the worker. */
        }
      } else if (!state->stopping && !stop.stop_requested()) {
        ++state->status.completed;
        try {
          if (publish)
            publish();
        } catch (...) {
          ++state->status.failed;
        }
      }
      state->cv.notify_all();
    }
    state->done = true;
    state->cv.notify_all();
  }

  std::shared_ptr<State> state_;
  std::thread worker_;
  bool joined_ = true;
};
} // namespace rocket
