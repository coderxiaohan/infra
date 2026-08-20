#pragma once

#include "evidence_warehouse_index_model.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace inferlab_evidence_warehouse {

struct SqliteWriteStatus {
  bool requested = false;
  bool enabled = false;
  bool written = false;
  std::string status = "not_requested";
  std::string fallback = "json_only";
  std::string runtime_version;
  std::string error;
  std::size_t record_count = 0U;
};

SqliteWriteStatus write_sqlite_index(
    const std::filesystem::path& sqlite_path,
    const WarehousePlan& plan);

}  // namespace inferlab_evidence_warehouse
