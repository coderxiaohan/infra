#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace inferlab::edge_runtime_matrix {

struct RuntimeRow;

struct GeneratedTextCapture {
  std::string text;
  std::optional<std::size_t> observed_bytes;
  std::string status = "not_observed";
};

void execute_runtime_row(
    const std::filesystem::path& resolved_model_path,
    const std::filesystem::path& resolved_tokenizer_path,
    const std::string& prompt, GeneratedTextCapture* generated_text,
    RuntimeRow* row);

}  // namespace inferlab::edge_runtime_matrix
