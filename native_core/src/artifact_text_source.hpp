#pragma once

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_artifact {

struct TextSource {
  std::string id;
  std::string path;
  std::string text;
  bool present = false;
  bool readable = false;
  std::string status = "missing";
  std::size_t read_issue_count = 0;
};

inline bool text_read_has_issue(const TextReadResult& read) {
  return !read.readable && read.status != "missing";
}

inline TextSource read_text_source(const std::string& id,
                                   const std::filesystem::path& path) {
  const auto read = read_text_with_status(path);
  TextSource source;
  source.id = id;
  source.path = path.generic_string();
  source.text = read.readable ? read.text : "";
  source.present = read.present;
  source.readable = read.readable;
  source.status = read.status;
  source.read_issue_count = text_read_has_issue(read) ? 1U : 0U;
  return source;
}

inline const TextSource* find_text_source_by_id(
    const std::vector<TextSource>& sources, const std::string& id) {
  for (const auto& source : sources) {
    if (source.id == id) return &source;
  }
  return nullptr;
}

inline const TextSource& require_unique_text_source_by_id(
    const std::vector<TextSource>& sources, const std::string& id) {
  const TextSource* result = nullptr;
  for (const auto& source : sources) {
    if (source.id != id) continue;
    if (result != nullptr) {
      throw std::logic_error("duplicate text source id: " + id);
    }
    result = &source;
  }
  if (result == nullptr) {
    throw std::logic_error("missing text source id: " + id);
  }
  return *result;
}

inline const std::string& source_text_by_id(
    const std::vector<TextSource>& sources, const std::string& id) {
  static const std::string empty;
  const TextSource* source = find_text_source_by_id(sources, id);
  return source == nullptr ? empty : source->text;
}

inline std::size_t source_read_issue_count(
    const std::vector<TextSource>& sources) {
  std::size_t count = 0;
  for (const auto& source : sources) {
    count = inferlab::internal::checked_add_size(
        count, source.read_issue_count, "artifact text source issue count");
  }
  return count;
}

}  // namespace inferlab_artifact
