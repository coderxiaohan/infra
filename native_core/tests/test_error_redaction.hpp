#pragma once

#include <cctype>
#include <string>

namespace inferlab_test {

inline bool is_path_token_char(const char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.' ||
         ch == '/' || ch == '\\' || ch == ':' || ch == '~';
}

inline bool looks_like_path_token(const std::string& token) {
  if (token.size() >= 2 && token[0] == '~' &&
      (token[1] == '/' || token[1] == '\\')) {
    return true;
  }
  if (token.size() >= 3 &&
      std::isalpha(static_cast<unsigned char>(token[0])) &&
      token[1] == ':' && (token[2] == '/' || token[2] == '\\')) {
    return true;
  }
  return token.find('/') != std::string::npos ||
         token.find('\\') != std::string::npos;
}

inline std::string redact_test_error(const std::string& message) {
  std::string redacted;
  redacted.reserve(message.size());
  std::size_t pos = 0;
  while (pos < message.size()) {
    if (!is_path_token_char(message[pos])) {
      redacted.push_back(message[pos]);
      ++pos;
      continue;
    }
    const std::size_t begin = pos;
    while (pos < message.size() && is_path_token_char(message[pos])) {
      ++pos;
    }
    const std::string token = message.substr(begin, pos - begin);
    redacted += looks_like_path_token(token) ? "<redacted-path>" : token;
  }
  return redacted;
}

}  // namespace inferlab_test
