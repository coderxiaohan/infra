#include "inferlab/advanced_serving_runtime_communication.hpp"

#include "kv_transport_replay_contract.hpp"

#include <stdexcept>

namespace inferlab {

const char* kv_transport_replay_backend_name(
    const KvTransportReplayBackend backend) {
  switch (backend) {
    case KvTransportReplayBackend::gpu_gpu_nvlink:
      return "gpu_gpu_nvlink";
    case KvTransportReplayBackend::gpu_cpu_pcie:
      return "gpu_cpu_pcie";
    case KvTransportReplayBackend::nvme_gds:
      return "nvme_gds";
    case KvTransportReplayBackend::rdma:
      return "rdma";
    case KvTransportReplayBackend::ucx:
      return "ucx";
    case KvTransportReplayBackend::tcp_fallback:
      return "tcp_fallback";
  }
  throw std::invalid_argument("unknown KV transport backend");
}

const char* kv_transport_timeline_phase_name(
    const KvTransportTimelinePhase phase) {
  switch (phase) {
    case KvTransportTimelinePhase::prepare_descriptor:
      return "prepare_descriptor";
    case KvTransportTimelinePhase::sender_wait:
      return "sender_wait";
    case KvTransportTimelinePhase::kv_transfer:
      return "kv_transfer";
    case KvTransportTimelinePhase::receiver_wait:
      return "receiver_wait";
    case KvTransportTimelinePhase::decode_blocked:
      return "decode_blocked";
  }
  throw std::invalid_argument("unknown KV transport timeline phase");
}

const char* kv_transport_timeline_resource_name(
    const KvTransportTimelinePhase phase) {
  switch (phase) {
    case KvTransportTimelinePhase::prepare_descriptor:
      return "control";
    case KvTransportTimelinePhase::sender_wait:
      return "sender";
    case KvTransportTimelinePhase::kv_transfer:
      return "network";
    case KvTransportTimelinePhase::receiver_wait:
      return "receiver";
    case KvTransportTimelinePhase::decode_blocked:
      return "decode_worker";
  }
  throw std::invalid_argument("unknown KV transport timeline resource");
}

KvTransportReplayAssumptions default_kv_transport_replay_assumptions() {
  KvTransportReplayAssumptions assumptions;
  assumptions.profile_name =
      "compiled_kv_transport_timeline_analytical_fixture_v1";
  return assumptions;
}

KvTransportReplayResult estimate_kv_transport_replay(
    const KvTransportReplayAssumptions& assumptions,
    const KvTransportReplayScenario& input_scenario) {
  return kv_transport_replay_contract::build_result(
      assumptions, input_scenario);
}

}  // namespace inferlab
