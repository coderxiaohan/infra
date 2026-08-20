#pragma once

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace inferlab::advanced_serving_backend_internal {

inline bool is_identifier_char(const unsigned char ch) {
  return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
}

inline bool is_resource_token_char(const unsigned char ch) {
  return is_identifier_char(ch) || ch == '/' || ch == ':';
}

inline std::string normalized_identifier(const std::string& value,
                                         const std::string& fallback,
                                         const char* name) {
  const std::string normalized = value.empty() ? fallback : value;
  if (normalized.empty()) {
    throw std::invalid_argument(std::string(name) + " must not be empty");
  }
  for (const unsigned char ch : normalized) {
    if (!is_identifier_char(ch)) {
      throw std::invalid_argument(std::string(name) +
                                  " contains an unsafe character");
    }
  }
  return normalized;
}

inline std::string normalized_resource_token(const std::string& value,
                                             const std::string& fallback,
                                             const char* name) {
  const std::string normalized = value.empty() ? fallback : value;
  if (normalized.empty()) {
    throw std::invalid_argument(std::string(name) + " must not be empty");
  }
  for (const unsigned char ch : normalized) {
    if (!is_resource_token_char(ch)) {
      throw std::invalid_argument(std::string(name) +
                                  " contains an unsafe command character");
    }
  }
  return normalized;
}

inline std::string normalized_http_base_url(const std::string& value,
                                            const std::string& fallback,
                                            const char* name) {
  std::string normalized = value.empty() ? fallback : value;
  while (!normalized.empty() && normalized.back() == '/') {
    normalized.pop_back();
  }
  const bool supported_scheme = normalized.rfind("http://", 0) == 0 ||
                                normalized.rfind("https://", 0) == 0;
  if (!supported_scheme) {
    throw std::invalid_argument(std::string(name) +
                                " must use http:// or https://");
  }
  const auto authority_offset = normalized.find("//") + 2U;
  const auto authority_end = normalized.find('/', authority_offset);
  if (authority_offset >= normalized.size() ||
      authority_end == authority_offset) {
    throw std::invalid_argument(std::string(name) +
                                " must include a non-empty authority");
  }
  const auto authority_limit =
      authority_end == std::string::npos ? normalized.size() : authority_end;
  const auto port_separator = normalized.find(':', authority_offset);
  const auto host_limit = port_separator < authority_limit
                              ? port_separator
                              : authority_limit;
  if (host_limit == authority_offset) {
    throw std::invalid_argument(std::string(name) +
                                " must include a non-empty host");
  }
  for (std::size_t index = authority_offset; index < host_limit; ++index) {
    const auto ch = static_cast<unsigned char>(normalized[index]);
    if (!is_identifier_char(ch)) {
      throw std::invalid_argument(std::string(name) +
                                  " contains an invalid authority");
    }
  }
  if (port_separator < authority_limit) {
    if (port_separator + 1U == authority_limit) {
      throw std::invalid_argument(std::string(name) +
                                  " must include a non-empty port");
    }
    for (std::size_t index = port_separator + 1U; index < authority_limit;
         ++index) {
      const auto ch = static_cast<unsigned char>(normalized[index]);
      if (std::isdigit(ch) == 0) {
        throw std::invalid_argument(std::string(name) +
                                    " contains an invalid port");
      }
    }
  }
  for (const unsigned char ch : normalized) {
    if (!is_resource_token_char(ch) && ch != '%' && ch != '~') {
      throw std::invalid_argument(std::string(name) +
                                  " contains an unsafe command character");
    }
  }
  return normalized;
}

}  // namespace inferlab::advanced_serving_backend_internal
