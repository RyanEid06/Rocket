#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>

namespace rocket {
// Borrowed streams must outlive this queue. Only this thread writes complete
// frames/log records. Enqueue never waits for transport capacity. Shutdown
// drains and joins: an arbitrary caller-supplied streambuf cannot safely be
// interrupted or detached. This transport drain is separate from the analysis
// deadline.
class LspOutputQueue {
public:
  LspOutputQueue(std::ostream &output, std::ostream &log,
                 std::size_t maximumBytes = 64U * 1024U * 1024U)
      : output_(output), log_(log), maximumBytes_(maximumBytes),
        worker_([this] { run(); }) {}
  ~LspOutputQueue() { shutdown(); }
  LspOutputQueue(const LspOutputQueue &) = delete;
  LspOutputQueue &operator=(const LspOutputQueue &) = delete;
  bool send(std::string text, bool log = false) {
    std::lock_guard lock(mutex_);
    if (stopping_ || failed_)
      return false;
    if (text.size() > maximumBytes_ - bytes_ || pending_.size() >= 1024) {
      failed_ = true;
      pending_.clear();
      bytes_ = 0;
      cv_.notify_all();
      return false;
    }
    bytes_ += text.size();
    pending_.push_back({std::move(text), log});
    cv_.notify_all();
    return true;
  }
  bool shutdown() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
      cv_.notify_all();
    }
    if (worker_.joinable())
      worker_.join();
    return !failed_;
  }

private:
  struct Record {
    std::string text;
    bool log;
  };
  void run() {
    std::unique_lock lock(mutex_);
    for (;;) {
      cv_.wait(lock, [&] { return stopping_ || failed_ || !pending_.empty(); });
      if (failed_ || (stopping_ && pending_.empty()))
        break;
      auto record = std::move(pending_.front());
      pending_.pop_front();
      bytes_ -= record.text.size();
      lock.unlock();
      bool ok = false;
      try {
        auto &stream = record.log ? log_ : output_;
        stream << record.text;
        stream.flush();
        ok = static_cast<bool>(stream);
      } catch (...) {
      }
      lock.lock();
      if (!ok) {
        failed_ = true;
        pending_.clear();
        bytes_ = 0;
      }
    }
  }
  std::ostream &output_, &log_;
  const std::size_t maximumBytes_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Record> pending_;
  std::size_t bytes_ = 0;
  bool stopping_ = false, failed_ = false;
  std::thread worker_;
};
} // namespace rocket
