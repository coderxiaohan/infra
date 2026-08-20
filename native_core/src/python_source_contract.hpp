#pragma once

#include <cstddef>
#include <string>

namespace inferlab_python_source {

struct ObjectProjection {
  std::size_t assignment_count = 0U;
  std::size_t object_count = 0U;
  bool syntax_valid = false;
  std::string object_text;

  bool usable() const {
    return assignment_count == 1U && object_count == 1U && syntax_valid &&
           !object_text.empty();
  }
};

ObjectProjection mapping_object(const std::string& document,
                                const std::string& assignment,
                                const std::string& key);

ObjectProjection sequence_object_by_string_field(
    const std::string& document,
    const std::string& assignment,
    char sequence_open,
    const std::string& field,
    const std::string& expected);

bool object_string_field_equals(const std::string& object_text,
                                const std::string& field,
                                const std::string& expected);

bool object_nonempty_string_list_field(const std::string& object_text,
                                       const std::string& field);

std::size_t object_list_string_count(const std::string& object_text,
                                     const std::string& field,
                                     const std::string& expected);

std::size_t assignment_string_literal_count(
    const std::string& document,
    const std::string& assignment,
    char container_open,
    const std::string& expected);

std::size_t matching_call_count(
    const std::string& document,
    const std::string& function,
    const std::string& expected_string,
    std::size_t required_string_count,
    const std::string& required_identifier = {});

std::size_t call_count(const std::string& document,
                       const std::string& function);

}  // namespace inferlab_python_source
