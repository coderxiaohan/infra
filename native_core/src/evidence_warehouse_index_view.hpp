#pragma once

#include "evidence_warehouse_index_model.hpp"
#include "evidence_warehouse_index_sqlite.hpp"

#include <filesystem>
#include <string>

namespace inferlab_evidence_warehouse {

std::string warehouse_artifact_json(
    const WarehousePlan& plan,
    const SqliteWriteStatus& sqlite_status,
    const std::filesystem::path& repo_root,
    const std::filesystem::path& results_root,
    const std::filesystem::path& output_path,
    const std::filesystem::path& sqlite_output_path,
    const std::string& format);

}  // namespace inferlab_evidence_warehouse
