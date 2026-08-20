#pragma once

#include "inferlab/edge_runtime.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace inferlab::edge_runtime_adapter {

namespace deadline_detail {
struct CooperativeGenerationDeadlineState;
}

using Clock = std::chrono::steady_clock;

inline double elapsed_ms(const Clock::time_point begin,
                         const Clock::time_point end) noexcept {
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

inline EdgeRuntimeOperationResult operation_result(
    const EdgeRuntimeStatus status, std::string category,
    std::string detail) {
  return {status, std::move(category), std::move(detail)};
}

class CooperativeGenerationDeadline final {
 public:
  CooperativeGenerationDeadline(
      std::uint64_t maximum_duration_ms,
      std::atomic<bool>* cancellation_requested);

  CooperativeGenerationDeadline(const CooperativeGenerationDeadline&) =
      delete;
  CooperativeGenerationDeadline& operator=(
      const CooperativeGenerationDeadline&) = delete;

  ~CooperativeGenerationDeadline();

  EdgeGenerationResult finish(EdgeGenerationResult result);

 private:
  bool stop() noexcept;

  std::shared_ptr<deadline_detail::CooperativeGenerationDeadlineState> state_;
  bool deadline_exceeded_ = false;
};

}  // namespace inferlab::edge_runtime_adapter
