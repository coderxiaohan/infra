#include "python_source_contract.hpp"

#include "checked_index.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_python_source {
namespace {

struct Slice {
  std::size_t begin = 0U;
  std::size_t end = 0U;
  bool present = false;
};

struct AssignmentContainer {
  std::size_t count = 0U;
  Slice value;
};

struct FieldLookup {
  std::size_t count = 0U;
  Slice value;
  bool syntax_valid = false;
};

struct ObjectLookup {
  std::size_t count = 0U;
  Slice object;
  bool syntax_valid = false;
};

struct SequenceObjects {
  std::vector<Slice> objects;
  bool syntax_valid = false;
};

bool identifier_char(const char value) {
  return (value >= 'a' && value <= 'z') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') || value == '_';
}

std::size_t skip_trivia(const std::string& document,
                        std::size_t position,
                        const std::size_t end) {
  while (position < end) {
    const char value = document[position];
    if (value == ' ' || value == '\t' || value == '\r' || value == '\n') {
      ++position;
      continue;
    }
    if (value != '#') break;
    const std::size_t line_end = document.find('\n', position + 1U);
    position = line_end == std::string::npos
                   ? end
                   : std::min(end, line_end + 1U);
  }
  return position;
}

bool parse_python_string(const std::string& document,
                         const std::size_t position,
                         const std::size_t end,
                         std::size_t& next,
                         std::string& value) {
  value.clear();
  if (position >= end ||
      (document[position] != '\'' && document[position] != '"')) {
    return false;
  }
  const char quote = document[position];
  const bool triple = position + 2U < end &&
                      document[position + 1U] == quote &&
                      document[position + 2U] == quote;
  std::size_t cursor = position + (triple ? 3U : 1U);
  while (cursor < end) {
    if (triple && cursor + 2U < end && document[cursor] == quote &&
        document[cursor + 1U] == quote &&
        document[cursor + 2U] == quote) {
      next = cursor + 3U;
      return true;
    }
    if (!triple && document[cursor] == quote) {
      next = cursor + 1U;
      return true;
    }
    if (document[cursor] != '\\') {
      value.push_back(document[cursor]);
      ++cursor;
      continue;
    }
    if (cursor + 1U >= end) return false;
    const char escaped = document[cursor + 1U];
    switch (escaped) {
      case 'n':
        value.push_back('\n');
        break;
      case 'r':
        value.push_back('\r');
        break;
      case 't':
        value.push_back('\t');
        break;
      default:
        value.push_back(escaped);
        break;
    }
    cursor += 2U;
  }
  return false;
}

char matching_close(const char open) {
  switch (open) {
    case '(':
      return ')';
    case '[':
      return ']';
    case '{':
      return '}';
    default:
      return '\0';
  }
}

std::size_t find_matching_container(const std::string& document,
                                    const std::size_t open,
                                    const std::size_t end) {
  if (open >= end || matching_close(document[open]) == '\0') {
    return std::string::npos;
  }
  std::vector<char> closes = {matching_close(document[open])};
  std::size_t cursor = open + 1U;
  while (cursor < end) {
    if (document[cursor] == '#') {
      const std::size_t line_end = document.find('\n', cursor + 1U);
      cursor = line_end == std::string::npos
                   ? end
                   : std::min(end, line_end + 1U);
      continue;
    }
    if (document[cursor] == '\'' || document[cursor] == '"') {
      std::size_t next = cursor;
      std::string ignored;
      if (!parse_python_string(document, cursor, end, next, ignored)) {
        return std::string::npos;
      }
      cursor = next;
      continue;
    }
    const char close = matching_close(document[cursor]);
    if (close != '\0') {
      closes.push_back(close);
      ++cursor;
      continue;
    }
    if (!closes.empty() && document[cursor] == closes.back()) {
      closes.pop_back();
      if (closes.empty()) return cursor;
      ++cursor;
      continue;
    }
    if (document[cursor] == ')' || document[cursor] == ']' ||
        document[cursor] == '}') {
      return std::string::npos;
    }
    ++cursor;
  }
  return std::string::npos;
}

std::size_t python_value_end(const std::string& document,
                             const std::size_t start,
                             const std::size_t end) {
  if (start >= end) return std::string::npos;
  if (document[start] == '\'' || document[start] == '"') {
    std::size_t next = start;
    std::string ignored;
    return parse_python_string(document, start, end, next, ignored)
               ? next
               : std::string::npos;
  }
  if (matching_close(document[start]) != '\0') {
    const std::size_t close = find_matching_container(document, start, end);
    return close == std::string::npos ? close : close + 1U;
  }
  std::size_t cursor = start;
  while (cursor < end && document[cursor] != ',' &&
         document[cursor] != '\n' && document[cursor] != '#') {
    ++cursor;
  }
  while (cursor > start &&
         (document[cursor - 1U] == ' ' || document[cursor - 1U] == '\t' ||
          document[cursor - 1U] == '\r')) {
    --cursor;
  }
  return cursor == start ? std::string::npos : cursor;
}

AssignmentContainer find_assignment_container(
    const std::string& document,
    const std::string& assignment,
    const char expected_open) {
  AssignmentContainer result;
  std::size_t cursor = 0U;
  while (cursor < document.size()) {
    if (document[cursor] == '#') {
      const std::size_t line_end = document.find('\n', cursor + 1U);
      cursor = line_end == std::string::npos ? document.size()
                                             : line_end + 1U;
      continue;
    }
    if (document[cursor] == '\'' || document[cursor] == '"') {
      std::size_t next = cursor;
      std::string ignored;
      if (!parse_python_string(document, cursor, document.size(), next,
                               ignored)) {
        return result;
      }
      cursor = next;
      continue;
    }
    if (!identifier_char(document[cursor])) {
      ++cursor;
      continue;
    }
    const std::size_t token_begin = cursor;
    while (cursor < document.size() && identifier_char(document[cursor])) {
      ++cursor;
    }
    if (document.substr(token_begin, cursor - token_begin) != assignment) {
      continue;
    }

    std::size_t scan = cursor;
    std::size_t annotation_depth = 0U;
    bool found_equals = false;
    while (scan < document.size() && document[scan] != '\n') {
      if (document[scan] == '\'' || document[scan] == '"') {
        std::size_t next = scan;
        std::string ignored;
        if (!parse_python_string(document, scan, document.size(), next,
                                 ignored)) {
          return result;
        }
        scan = next;
        continue;
      }
      if (document[scan] == '#' && annotation_depth == 0U) break;
      if (document[scan] == '[' || document[scan] == '(' ||
          document[scan] == '{') {
        ++annotation_depth;
      } else if (document[scan] == ']' || document[scan] == ')' ||
                 document[scan] == '}') {
        if (annotation_depth == 0U) break;
        --annotation_depth;
      } else if (document[scan] == '=' && annotation_depth == 0U &&
                 (scan + 1U >= document.size() ||
                  document[scan + 1U] != '=')) {
        found_equals = true;
        ++scan;
        break;
      }
      ++scan;
    }
    if (!found_equals) continue;
    scan = skip_trivia(document, scan, document.size());
    if (scan >= document.size() || document[scan] != expected_open) continue;
    const std::size_t close =
        find_matching_container(document, scan, document.size());
    if (close == std::string::npos) continue;
    result.count = inferlab::internal::checked_add_size(
        result.count, 1U, "Python assignment container match count");
    if (result.count == 1U) result.value = {scan, close + 1U, true};
    cursor = close + 1U;
  }
  if (result.count != 1U) result.value = {};
  return result;
}

FieldLookup find_mapping_field(const std::string& document,
                               const Slice& mapping,
                               const std::string& field) {
  FieldLookup result;
  if (!mapping.present || mapping.end <= mapping.begin + 1U ||
      document[mapping.begin] != '{' ||
      document[mapping.end - 1U] != '}') {
    return result;
  }
  const std::size_t content_end = mapping.end - 1U;
  std::size_t cursor = mapping.begin + 1U;
  while (true) {
    cursor = skip_trivia(document, cursor, content_end);
    if (cursor >= content_end) {
      result.syntax_valid = true;
      return result;
    }
    if (document[cursor] == ',') {
      ++cursor;
      continue;
    }
    std::size_t after_key = cursor;
    std::string key;
    if (!parse_python_string(document, cursor, content_end, after_key, key)) {
      return result;
    }
    cursor = skip_trivia(document, after_key, content_end);
    if (cursor >= content_end || document[cursor] != ':') return result;
    cursor = skip_trivia(document, cursor + 1U, content_end);
    const std::size_t value_end =
        python_value_end(document, cursor, content_end);
    if (value_end == std::string::npos || value_end > content_end) {
      return result;
    }
    if (key == field) {
      result.count = inferlab::internal::checked_add_size(
          result.count, 1U, "Python mapping field match count");
      if (result.count == 1U) result.value = {cursor, value_end, true};
    }
    cursor = value_end;
  }
}

ObjectLookup find_mapping_object(const std::string& document,
                                 const Slice& mapping,
                                 const std::string& key) {
  const FieldLookup field = find_mapping_field(document, mapping, key);
  ObjectLookup result;
  result.count = field.count;
  result.syntax_valid = field.syntax_valid;
  if (field.count == 1U && field.value.present &&
      document[field.value.begin] == '{') {
    result.object = field.value;
  }
  return result;
}

bool object_string_field_equals_impl(const std::string& document,
                                     const Slice& object,
                                     const std::string& field,
                                     const std::string& expected) {
  const FieldLookup lookup = find_mapping_field(document, object, field);
  if (!lookup.syntax_valid || lookup.count != 1U || !lookup.value.present) {
    return false;
  }
  std::size_t next = lookup.value.begin;
  std::string actual;
  return parse_python_string(document, lookup.value.begin, lookup.value.end,
                             next, actual) &&
         next == lookup.value.end && actual == expected;
}

std::size_t string_literal_count(const std::string& document,
                                 const Slice& value,
                                 const std::string& expected) {
  if (!value.present) return 0U;
  std::size_t count = 0U;
  std::size_t cursor = value.begin;
  while (cursor < value.end) {
    if (document[cursor] == '#') {
      const std::size_t line_end = document.find('\n', cursor + 1U);
      cursor = line_end == std::string::npos
                   ? value.end
                   : std::min(value.end, line_end + 1U);
      continue;
    }
    if (document[cursor] != '\'' && document[cursor] != '"') {
      ++cursor;
      continue;
    }
    std::size_t next = cursor;
    std::string actual;
    if (!parse_python_string(document, cursor, value.end, next, actual)) {
      return 0U;
    }
    if (actual == expected) {
      count = inferlab::internal::checked_add_size(
          count, 1U, "Python string literal match count");
    }
    cursor = next;
  }
  return count;
}

bool object_nonempty_string_list_field_impl(const std::string& document,
                                            const Slice& object,
                                            const std::string& field) {
  const FieldLookup lookup = find_mapping_field(document, object, field);
  if (!lookup.syntax_valid || lookup.count != 1U || !lookup.value.present ||
      document[lookup.value.begin] != '[') {
    return false;
  }
  std::size_t string_count = 0U;
  std::size_t cursor = lookup.value.begin + 1U;
  const std::size_t end = lookup.value.end - 1U;
  while (true) {
    cursor = skip_trivia(document, cursor, end);
    if (cursor >= end) return string_count > 0U;
    if (document[cursor] == ',') {
      ++cursor;
      continue;
    }
    std::size_t next = cursor;
    std::string ignored;
    if (!parse_python_string(document, cursor, end, next, ignored)) {
      return false;
    }
    string_count = inferlab::internal::checked_add_size(
        string_count, 1U, "Python nonempty string-list item count");
    cursor = next;
  }
}

SequenceObjects top_level_sequence_objects(const std::string& document,
                                           const Slice& sequence) {
  SequenceObjects result;
  const bool valid_delimiters =
      sequence.present && sequence.end > sequence.begin + 1U &&
      ((document[sequence.begin] == '[' &&
        document[sequence.end - 1U] == ']') ||
       (document[sequence.begin] == '(' &&
        document[sequence.end - 1U] == ')'));
  if (!valid_delimiters) return result;
  const std::size_t end = sequence.end - 1U;
  std::size_t cursor = sequence.begin + 1U;
  while (true) {
    cursor = skip_trivia(document, cursor, end);
    if (cursor >= end) {
      result.syntax_valid = true;
      return result;
    }
    if (document[cursor] == ',') {
      ++cursor;
      continue;
    }
    const std::size_t value_end = python_value_end(document, cursor, end);
    if (value_end == std::string::npos || value_end > end) return {};
    if (document[cursor] == '{') {
      result.objects.push_back({cursor, value_end, true});
    }
    cursor = value_end;
  }
}

ObjectLookup find_sequence_object_by_string_field_impl(
    const std::string& document,
    const Slice& sequence,
    const std::string& field,
    const std::string& expected) {
  ObjectLookup result;
  const SequenceObjects objects =
      top_level_sequence_objects(document, sequence);
  result.syntax_valid = objects.syntax_valid;
  if (!objects.syntax_valid) return result;
  for (const auto& object : objects.objects) {
    if (!object_string_field_equals_impl(document, object, field, expected)) {
      continue;
    }
    result.count = inferlab::internal::checked_add_size(
        result.count, 1U, "Python sequence object match count");
    if (result.count == 1U) result.object = object;
  }
  if (result.count != 1U) result.object = {};
  return result;
}

struct CallBodies {
  std::vector<Slice> bodies;
  bool syntax_valid = true;
};

CallBodies find_call_bodies(const std::string& document,
                            const std::string& function) {
  CallBodies result;
  std::size_t cursor = 0U;
  while (cursor < document.size()) {
    if (document[cursor] == '#') {
      const std::size_t line_end = document.find('\n', cursor + 1U);
      cursor = line_end == std::string::npos ? document.size()
                                             : line_end + 1U;
      continue;
    }
    if (document[cursor] == '\'' || document[cursor] == '"') {
      std::size_t next = cursor;
      std::string ignored;
      if (!parse_python_string(document, cursor, document.size(), next,
                               ignored)) {
        result.syntax_valid = false;
        result.bodies.clear();
        return result;
      }
      cursor = next;
      continue;
    }
    if (!identifier_char(document[cursor])) {
      ++cursor;
      continue;
    }
    const std::size_t token_begin = cursor;
    while (cursor < document.size() && identifier_char(document[cursor])) {
      ++cursor;
    }
    if (document.substr(token_begin, cursor - token_begin) != function) {
      continue;
    }
    const std::size_t open = skip_trivia(document, cursor, document.size());
    if (open >= document.size() || document[open] != '(') continue;
    const std::size_t close =
        find_matching_container(document, open, document.size());
    if (close == std::string::npos) {
      result.syntax_valid = false;
      result.bodies.clear();
      return result;
    }
    result.bodies.push_back({open, close + 1U, true});
    cursor = close + 1U;
  }
  return result;
}

bool slice_has_identifier(const std::string& document,
                          const Slice& slice,
                          const std::string& identifier) {
  std::size_t cursor = slice.begin;
  while (cursor < slice.end) {
    if (document[cursor] == '#') {
      const std::size_t line_end = document.find('\n', cursor + 1U);
      cursor = line_end == std::string::npos
                   ? slice.end
                   : std::min(slice.end, line_end + 1U);
      continue;
    }
    if (document[cursor] == '\'' || document[cursor] == '"') {
      std::size_t next = cursor;
      std::string ignored;
      if (!parse_python_string(document, cursor, slice.end, next, ignored)) {
        return false;
      }
      cursor = next;
      continue;
    }
    if (!identifier_char(document[cursor])) {
      ++cursor;
      continue;
    }
    const std::size_t begin = cursor;
    while (cursor < slice.end && identifier_char(document[cursor])) ++cursor;
    if (document.substr(begin, cursor - begin) == identifier) return true;
  }
  return false;
}

Slice whole_object(const std::string& object_text) {
  return {0U, object_text.size(), !object_text.empty()};
}

}  // namespace

ObjectProjection mapping_object(const std::string& document,
                                const std::string& assignment,
                                const std::string& key) {
  ObjectProjection projection;
  const AssignmentContainer container =
      find_assignment_container(document, assignment, '{');
  projection.assignment_count = container.count;
  if (container.count != 1U || !container.value.present) return projection;
  const ObjectLookup object =
      find_mapping_object(document, container.value, key);
  projection.object_count = object.count;
  projection.syntax_valid = object.syntax_valid;
  if (object.count == 1U && object.object.present) {
    projection.object_text = document.substr(
        object.object.begin, object.object.end - object.object.begin);
  }
  return projection;
}

ObjectProjection sequence_object_by_string_field(
    const std::string& document,
    const std::string& assignment,
    const char sequence_open,
    const std::string& field,
    const std::string& expected) {
  ObjectProjection projection;
  const AssignmentContainer container =
      find_assignment_container(document, assignment, sequence_open);
  projection.assignment_count = container.count;
  if (container.count != 1U || !container.value.present) return projection;
  const ObjectLookup object = find_sequence_object_by_string_field_impl(
      document, container.value, field, expected);
  projection.object_count = object.count;
  projection.syntax_valid = object.syntax_valid;
  if (object.count == 1U && object.object.present) {
    projection.object_text = document.substr(
        object.object.begin, object.object.end - object.object.begin);
  }
  return projection;
}

bool object_string_field_equals(const std::string& object_text,
                                const std::string& field,
                                const std::string& expected) {
  return object_string_field_equals_impl(
      object_text, whole_object(object_text), field, expected);
}

bool object_nonempty_string_list_field(const std::string& object_text,
                                       const std::string& field) {
  return object_nonempty_string_list_field_impl(
      object_text, whole_object(object_text), field);
}

std::size_t object_list_string_count(const std::string& object_text,
                                     const std::string& field,
                                     const std::string& expected) {
  const FieldLookup lookup =
      find_mapping_field(object_text, whole_object(object_text), field);
  if (!lookup.syntax_valid || lookup.count != 1U || !lookup.value.present ||
      object_text[lookup.value.begin] != '[') {
    return 0U;
  }
  return string_literal_count(object_text, lookup.value, expected);
}

std::size_t assignment_string_literal_count(
    const std::string& document,
    const std::string& assignment,
    const char container_open,
    const std::string& expected) {
  const AssignmentContainer container =
      find_assignment_container(document, assignment, container_open);
  return container.count == 1U && container.value.present
             ? string_literal_count(document, container.value, expected)
             : 0U;
}

std::size_t matching_call_count(
    const std::string& document,
    const std::string& function,
    const std::string& expected_string,
    const std::size_t required_string_count,
    const std::string& required_identifier) {
  const CallBodies calls = find_call_bodies(document, function);
  if (!calls.syntax_valid) return 0U;
  std::size_t count = 0U;
  for (const auto& body : calls.bodies) {
    if (string_literal_count(document, body, expected_string) !=
        required_string_count) {
      continue;
    }
    if (!required_identifier.empty() &&
        !slice_has_identifier(document, body, required_identifier)) {
      continue;
    }
    count = inferlab::internal::checked_add_size(
        count, 1U, "Python matching call count");
  }
  return count;
}

std::size_t call_count(const std::string& document,
                       const std::string& function) {
  const CallBodies calls = find_call_bodies(document, function);
  return calls.syntax_valid ? calls.bodies.size() : 0U;
}

}  // namespace inferlab_python_source
