#pragma once
#include "language_server.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <sstream>
#include <streambuf>
#include <thread>

namespace rocket::test {
inline std::string lspFrame(const std::string &body) {
  return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}
class InputPipe : public std::streambuf {
public:
  void put(const std::string &text) {
    std::lock_guard lock(mutex_);
    data_.insert(data_.end(), text.begin(), text.end());
    cv_.notify_all();
  }
  void close() {
    std::lock_guard lock(mutex_);
    closed_ = true;
    cv_.notify_all();
  }

protected:
  int_type underflow() override {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return closed_ || !data_.empty(); });
    if (data_.empty())
      return traits_type::eof();
    current_ = data_.front();
    data_.pop_front();
    setg(&current_, &current_, &current_ + 1);
    return traits_type::to_int_type(current_);
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<char> data_;
  bool closed_ = false;
  char current_{};
};
class OutputPipe : public std::streambuf {
public:
  std::string
  next(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
    std::unique_lock lock(mutex_);
    if (!cv_.wait_for(lock, timeout, [&] { return !messages_.empty(); }))
      throw std::runtime_error("timed out waiting for LSP output");
    auto result = std::move(messages_.front());
    messages_.pop_front();
    return result;
  }

protected:
  std::streamsize xsputn(const char *text, std::streamsize count) override {
    std::lock_guard lock(mutex_);
    bytes_.append(text, static_cast<size_t>(count));
    extract();
    return count;
  }
  int_type overflow(int_type value) override {
    if (traits_type::eq_int_type(value, traits_type::eof()))
      return traits_type::not_eof(value);
    const char c = traits_type::to_char_type(value);
    xsputn(&c, 1);
    return value;
  }

private:
  void extract() {
    for (;;) {
      const auto end = bytes_.find("\r\n\r\n");
      if (end == std::string::npos)
        break;
      if (!bytes_.starts_with("Content-Length: "))
        throw std::runtime_error("interleaved LSP frame");
      const auto size = std::stoull(bytes_.substr(16, end - 16));
      if (bytes_.size() < end + 4 + size)
        break;
      messages_.push_back(bytes_.substr(end + 4, size));
      bytes_.erase(0, end + 4 + size);
      cv_.notify_all();
    }
  }
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::string> messages_;
  std::string bytes_;
};
class LspSession {
public:
  explicit LspSession(AnalysisQueue::Executor executor = {})
      : input_(&inputBuffer_), output_(&outputBuffer_),
        result_(
            std::async(std::launch::async, [&, executor = std::move(executor)] {
              LanguageServer server(input_, output_, log_, executor);
              return server.run();
            })) {}
  ~LspSession() {
    inputBuffer_.close();
    if (result_.valid())
      result_.wait();
  }
  void send(const std::string &body) { inputBuffer_.put(lspFrame(body)); }
  std::string response(const std::string &id) {
    for (;;) {
      auto body = outputBuffer_.next();
      if (body.find("\"id\":\"" + id + "\"") != std::string::npos)
        return body;
      recorded += lspFrame(body);
    }
  }
  void settle() {
    for (int attempt = 0; attempt < 1000; ++attempt) {
      send(
          R"({"jsonrpc":"2.0","id":"barrier","method":"rocket/projectStatus"})");
      const auto status = response("barrier");
      if (status.find("\"analysisPending\":true") == std::string::npos)
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("analysis did not settle");
  }
  int finish() {
    inputBuffer_.close();
    return result_.get();
  }
  std::string recorded;
  std::string log() const { return log_.str(); }

private:
  InputPipe inputBuffer_;
  OutputPipe outputBuffer_;
  std::istream input_;
  std::ostream output_;
  std::ostringstream log_;
  std::future<int> result_;
};

// Existing fixtures are conversational clients, not an entire session already
// buffered up to shutdown. Barriers live only in this test transport.
class TranscriptServer {
public:
  TranscriptServer(std::istream &input, std::ostream &output, std::ostream &log)
      : input_(input), output_(output), log_(log) {}
  int run() {
    LspSession session;
    std::string header;
    while (std::getline(input_, header)) {
      const auto length = std::stoull(header.substr(16));
      std::getline(input_, header);
      std::string body(length, '\0');
      input_.read(body.data(), length);
      session.send(body);
      if (body.find("\"method\":\"exit\"") != std::string::npos)
        break;
      if (body.find("\"method\":\"shutdown\"") != std::string::npos) {
        // Consume shutdown response before exit.
        session.send(
            R"({"jsonrpc":"2.0","id":"barrier","method":"rocket/projectStatus"})");
        session.response("barrier");
      } else
        session.settle();
    }
    const int result = session.finish();
    output_ << session.recorded;
    log_ << session.log();
    return result;
  }

private:
  std::istream &input_;
  std::ostream &output_;
  std::ostream &log_;
};
} // namespace rocket::test
