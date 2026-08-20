#pragma once

#include "inferlab/edge_cloud.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace inferlab {

struct OpenAiCompatibleTransportConfig {
  std::string product_id;
  std::string endpoint_url;
  std::string api_key_environment_variable;
  bool allow_loopback_http = false;
  std::uint64_t connect_timeout_ms = 5000U;
  std::size_t maximum_event_bytes = 1024U * 1024U;
  std::size_t maximum_response_bytes = 64U * 1024U * 1024U;
};

void validate_openai_compatible_transport_config(
    const OpenAiCompatibleTransportConfig& config);
std::unique_ptr<ICloudInferenceTransport>
create_openai_compatible_cloud_transport(
    const OpenAiCompatibleTransportConfig& config);

}  // namespace inferlab
