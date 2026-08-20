#include "edge_runtime_adapter_utils.hpp"

#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace inferlab::edge_runtime_adapter {
namespace deadline_detail {

struct CooperativeGenerationDeadlineState {
  std::mutex mutex;
  std::atomic<bool>* cancellation_requested = nullptr;
  std::uint64_t registration_id = 0U;
  bool generation_finished = false;
  bool deadline_exceeded = false;
};

namespace {

struct DeadlineEntry {
  std::uint64_t registration_id = 0U;
  std::shared_ptr<CooperativeGenerationDeadlineState> state;
};

class CooperativeDeadlineScheduler final {
 public:
  CooperativeDeadlineScheduler() : worker_([this] { run(); }) {}

  CooperativeDeadlineScheduler(const CooperativeDeadlineScheduler&) = delete;
  CooperativeDeadlineScheduler& operator=(
      const CooperativeDeadlineScheduler&) = delete;

  ~CooperativeDeadlineScheduler() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable()) worker_.join();
  }

  void schedule(
      const std::shared_ptr<CooperativeGenerationDeadlineState>& state,
      const std::uint64_t maximum_duration_ms) {
    if (!state || maximum_duration_ms == 0U) {
      throw std::invalid_argument(
          "edge cooperative deadline requires state and duration");
    }

    const auto deadline =
        Clock::now() + std::chrono::milliseconds(maximum_duration_ms);
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      throw std::runtime_error("edge cooperative deadline scheduler stopped");
    }
    if (next_registration_id_ ==
        std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error(
          "edge cooperative deadline registration id exhausted");
    }

    const std::uint64_t registration_id = next_registration_id_++;
    state->registration_id = registration_id;
    const auto iterator = deadlines_.emplace(
        deadline, DeadlineEntry{registration_id, state});
    try {
      const auto inserted = registrations_.emplace(registration_id, iterator);
      if (!inserted.second) {
        throw std::logic_error(
            "edge cooperative deadline registration id collision");
      }
    } catch (...) {
      deadlines_.erase(iterator);
      throw;
    }
    condition_.notify_all();
  }

  void cancel(const std::uint64_t registration_id) noexcept {
    if (registration_id == 0U) return;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto found = registrations_.find(registration_id);
      if (found == registrations_.end()) return;
      deadlines_.erase(found->second);
      registrations_.erase(found);
    }
    condition_.notify_all();
  }

 private:
  using DeadlineMap =
      std::multimap<Clock::time_point, DeadlineEntry>;

  void run() noexcept {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopping_) {
      if (deadlines_.empty()) {
        condition_.wait(lock, [this] {
          return stopping_ || !deadlines_.empty();
        });
        continue;
      }

      const auto deadline = deadlines_.begin()->first;
      if (Clock::now() < deadline) {
        condition_.wait_until(lock, deadline);
        continue;
      }

      auto iterator = deadlines_.begin();
      DeadlineEntry entry = std::move(iterator->second);
      registrations_.erase(entry.registration_id);
      deadlines_.erase(iterator);

      lock.unlock();
      {
        std::lock_guard<std::mutex> state_lock(entry.state->mutex);
        if (!entry.state->generation_finished) {
          entry.state->deadline_exceeded = true;
          entry.state->cancellation_requested->store(
              true, std::memory_order_relaxed);
        }
      }
      lock.lock();
    }
  }

  std::mutex mutex_;
  std::condition_variable condition_;
  DeadlineMap deadlines_;
  std::unordered_map<std::uint64_t, DeadlineMap::iterator> registrations_;
  std::uint64_t next_registration_id_ = 1U;
  bool stopping_ = false;
  std::thread worker_;
};

CooperativeDeadlineScheduler& deadline_scheduler() {
  static CooperativeDeadlineScheduler scheduler;
  return scheduler;
}

}  // namespace
}  // namespace deadline_detail

CooperativeGenerationDeadline::CooperativeGenerationDeadline(
    const std::uint64_t maximum_duration_ms,
    std::atomic<bool>* const cancellation_requested) {
  if (maximum_duration_ms == 0U ||
      maximum_duration_ms > kMaximumEdgeGenerationDurationMs ||
      cancellation_requested == nullptr) {
    throw std::invalid_argument(
        "edge cooperative deadline requires bounded duration and cancellation state");
  }

  auto state =
      std::make_shared<deadline_detail::CooperativeGenerationDeadlineState>();
  state->cancellation_requested = cancellation_requested;
  deadline_detail::deadline_scheduler().schedule(state, maximum_duration_ms);
  state_ = std::move(state);
}

CooperativeGenerationDeadline::~CooperativeGenerationDeadline() { stop(); }

EdgeGenerationResult CooperativeGenerationDeadline::finish(
    EdgeGenerationResult result) {
  if (!stop()) return result;
  result.status = EdgeRuntimeStatus::kCancelled;
  result.error_category = "generation_deadline_exceeded";
  result.detail =
      "cooperative generation deadline elapsed; hard termination is not guaranteed";
  result.finish_reason = "cancelled";
  return result;
}

bool CooperativeGenerationDeadline::stop() noexcept {
  if (!state_) return deadline_exceeded_;

  std::uint64_t registration_id = 0U;
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    registration_id = state_->registration_id;
  }
  deadline_detail::deadline_scheduler().cancel(registration_id);

  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->generation_finished = true;
    deadline_exceeded_ = state_->deadline_exceeded;
  }
  state_.reset();
  return deadline_exceeded_;
}

}  // namespace inferlab::edge_runtime_adapter
