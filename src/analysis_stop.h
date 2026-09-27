#pragma once

// The macOS 14 SDK's libc++ does not provide C++20 stop_token. Keep the
// standard implementation on other hosts and use the same cooperative stop
// operations for Rocket's macOS analysis worker.
#if defined(__APPLE__)
#include <atomic>
#include <memory>
#include <utility>

namespace rocket {
class StopSource;
class StopToken {
public:
  bool stop_requested() const noexcept {
    return state_ && state_->load(std::memory_order_acquire);
  }

private:
  friend class StopSource;
  explicit StopToken(std::shared_ptr<std::atomic<bool>> state)
      : state_(std::move(state)) {}
  std::shared_ptr<std::atomic<bool>> state_;
};

class StopSource {
public:
  StopSource() : state_(std::make_shared<std::atomic<bool>>(false)) {}
  bool request_stop() noexcept {
    return !state_->exchange(true, std::memory_order_acq_rel);
  }
  StopToken get_token() const noexcept { return StopToken(state_); }

private:
  std::shared_ptr<std::atomic<bool>> state_;
};
} // namespace rocket
#else
#include <stop_token>
namespace rocket {
using StopSource = std::stop_source;
using StopToken = std::stop_token;
} // namespace rocket
#endif
