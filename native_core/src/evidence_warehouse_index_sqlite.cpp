#include "evidence_warehouse_index_sqlite.hpp"

#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#if defined(INFERLAB_HAS_SQLITE) && INFERLAB_HAS_SQLITE
#include <sqlite3.h>
#endif

namespace inferlab_evidence_warehouse {
namespace {

namespace fs = std::filesystem;

std::string redact_sqlite_error(const std::string& message,
                                const fs::path& path) {
  return inferlab_plan::redact_error_message(
      message, path.empty() ? std::vector<std::string>{}
                            : std::vector<std::string>{path.generic_string()});
}

bool sqlite_path_is_safe(const fs::path& path) {
  std::error_code error;
  const auto destination_status = fs::symlink_status(path, error);
  if (error) return false;
  if (fs::exists(destination_status) &&
      (!fs::is_regular_file(destination_status) ||
       fs::is_symlink(destination_status))) {
    return false;
  }

  fs::path current = path.parent_path();
  while (!current.empty()) {
    const auto status = fs::symlink_status(current, error);
    if (error) return false;
    if (fs::exists(status) &&
        (fs::is_symlink(status) || !fs::is_directory(status))) {
      return false;
    }
    const fs::path parent = current.parent_path();
    if (parent == current) break;
    current = parent;
  }
  return true;
}

#if defined(INFERLAB_HAS_SQLITE) && INFERLAB_HAS_SQLITE

bool execute_sql(sqlite3* db, const char* sql, std::string& error) {
  char* raw_error = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &raw_error) == SQLITE_OK) {
    return true;
  }
  error = raw_error == nullptr ? sqlite3_errmsg(db) : raw_error;
  sqlite3_free(raw_error);
  return false;
}

bool bind_text(sqlite3_stmt* statement, const int index,
               const std::string& value) {
  return sqlite3_bind_text(statement, index, value.c_str(), -1,
                           SQLITE_TRANSIENT) == SQLITE_OK;
}

bool bind_bool(sqlite3_stmt* statement, const int index, const bool value) {
  return sqlite3_bind_int(statement, index, value ? 1 : 0) == SQLITE_OK;
}

#endif

}  // namespace

SqliteWriteStatus write_sqlite_index(const fs::path& sqlite_path,
                                     const WarehousePlan& plan) {
  SqliteWriteStatus status;
  status.requested = !sqlite_path.empty();
  if (!status.requested) return status;

#if defined(INFERLAB_HAS_SQLITE) && INFERLAB_HAS_SQLITE
  status.enabled = true;
  status.status = "configured";
  status.runtime_version = sqlite3_libversion();
  const int metadata_record_count = inferlab::internal::checked_int_from_size(
      plan.lineage.records.size(), "SQLite warehouse record count");

  if (!sqlite_path_is_safe(sqlite_path)) {
    status.status = "sqlite_unsafe_path";
    status.error = "SQLite output must be a non-symlink regular file path under non-symlink directories";
    return status;
  }

  if (sqlite_path.has_parent_path()) {
    std::error_code directory_error;
    fs::create_directories(sqlite_path.parent_path(), directory_error);
    if (directory_error) {
      status.status = "sqlite_parent_create_failed";
      status.error = redact_sqlite_error(directory_error.message(),
                                         sqlite_path);
      return status;
    }
  }

  sqlite3* db = nullptr;
  const int open_status = sqlite3_open_v2(
      sqlite_path.string().c_str(), &db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
      nullptr);
  if (open_status != SQLITE_OK) {
    status.status = "sqlite_open_failed";
    status.error = redact_sqlite_error(
        db == nullptr ? "failed to open SQLite evidence warehouse"
                      : sqlite3_errmsg(db),
        sqlite_path);
    if (db != nullptr) sqlite3_close(db);
    return status;
  }

  std::string sqlite_error;
  const char* schema_sql =
      "BEGIN IMMEDIATE;"
      "DROP TABLE IF EXISTS artifacts;"
      "DROP TABLE IF EXISTS warehouse_metadata;"
      "CREATE TABLE warehouse_metadata("
      "schema_version INTEGER NOT NULL, warehouse_decision TEXT NOT NULL, "
      "record_count INTEGER NOT NULL);"
      "CREATE TABLE artifacts("
      "id TEXT PRIMARY KEY, artifact TEXT NOT NULL UNIQUE, "
      "target TEXT NOT NULL, report TEXT NOT NULL, evidence_level TEXT NOT NULL, "
      "claim_boundary TEXT NOT NULL, artifact_path_status TEXT NOT NULL, "
      "present INTEGER NOT NULL, parse_ok INTEGER NOT NULL, "
      "current_output_deferred INTEGER NOT NULL, "
      "producer_target_present INTEGER NOT NULL, matrix_present INTEGER NOT NULL, "
      "summary_present INTEGER NOT NULL, prometheus_present INTEGER NOT NULL, "
      "report_present INTEGER NOT NULL, final_report_present INTEGER NOT NULL, "
      "contract_present INTEGER NOT NULL, freshness_present INTEGER NOT NULL, "
      "evidence_bundle_present INTEGER NOT NULL);";
  if (!execute_sql(db, schema_sql, sqlite_error)) {
    status.status = "sqlite_schema_failed";
    status.error = redact_sqlite_error(sqlite_error, sqlite_path);
    std::string ignored;
    execute_sql(db, "ROLLBACK;", ignored);
    sqlite3_close(db);
    return status;
  }

  sqlite3_stmt* metadata_statement = nullptr;
  const char* metadata_sql =
      "INSERT INTO warehouse_metadata(schema_version, warehouse_decision, "
      "record_count) VALUES(?, ?, ?);";
  if (sqlite3_prepare_v2(db, metadata_sql, -1, &metadata_statement, nullptr) !=
      SQLITE_OK) {
    status.status = "sqlite_prepare_failed";
    status.error = redact_sqlite_error(sqlite3_errmsg(db), sqlite_path);
    std::string ignored;
    execute_sql(db, "ROLLBACK;", ignored);
    sqlite3_close(db);
    return status;
  }
  const std::string decision = warehouse_decision_name(plan.summary.decision);
  const bool metadata_bound =
      sqlite3_bind_int(metadata_statement, 1, 3) == SQLITE_OK &&
      bind_text(metadata_statement, 2, decision) &&
      sqlite3_bind_int(metadata_statement, 3, metadata_record_count) ==
          SQLITE_OK;
  if (!metadata_bound || sqlite3_step(metadata_statement) != SQLITE_DONE) {
    status.status = "sqlite_metadata_insert_failed";
    status.error = redact_sqlite_error(sqlite3_errmsg(db), sqlite_path);
    sqlite3_finalize(metadata_statement);
    std::string ignored;
    execute_sql(db, "ROLLBACK;", ignored);
    sqlite3_close(db);
    return status;
  }
  sqlite3_finalize(metadata_statement);

  const char* insert_sql =
      "INSERT INTO artifacts("
      "id, artifact, target, report, evidence_level, claim_boundary, "
      "artifact_path_status, present, parse_ok, current_output_deferred, "
      "producer_target_present, matrix_present, summary_present, "
      "prometheus_present, report_present, final_report_present, "
      "contract_present, freshness_present, evidence_bundle_present) "
      "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(db, insert_sql, -1, &statement, nullptr) !=
      SQLITE_OK) {
    status.status = "sqlite_prepare_failed";
    status.error = redact_sqlite_error(sqlite3_errmsg(db), sqlite_path);
    std::string ignored;
    execute_sql(db, "ROLLBACK;", ignored);
    sqlite3_close(db);
    return status;
  }

  for (const auto& record : plan.lineage.records) {
    if (record.spec == nullptr) {
      status.status = "sqlite_record_contract_failed";
      status.error = "warehouse record has no catalog specification";
      sqlite3_finalize(statement);
      std::string ignored;
      execute_sql(db, "ROLLBACK;", ignored);
      sqlite3_close(db);
      return status;
    }
    const auto& spec = *record.spec;
    const auto& warehouse = record.warehouse_record;
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
    const bool bound =
        bind_text(statement, 1, spec.id) &&
        bind_text(statement, 2, spec.artifact) &&
        bind_text(statement, 3, spec.target) &&
        bind_text(statement, 4, spec.report) &&
        bind_text(statement, 5, spec.evidence_level) &&
        bind_text(statement, 6,
                  inferlab_catalog::registry_claim_boundary(spec)) &&
        bind_text(statement, 7, record.artifact_path_status) &&
        bind_bool(statement, 8, warehouse.present) &&
        bind_bool(statement, 9, warehouse.metadata.parse_ok) &&
        bind_bool(statement, 10, record.current_output_deferred) &&
        bind_bool(statement, 11, record.producer_target_present) &&
        bind_bool(statement, 12, warehouse.matrix_present) &&
        bind_bool(statement, 13, warehouse.summary_present) &&
        bind_bool(statement, 14, warehouse.prometheus_present) &&
        bind_bool(statement, 15, warehouse.report_present) &&
        bind_bool(statement, 16, warehouse.final_report_present) &&
        bind_bool(statement, 17, warehouse.contract_present) &&
        bind_bool(statement, 18, warehouse.freshness_present) &&
        bind_bool(statement, 19, record.evidence_bundle_present);
    if (!bound || sqlite3_step(statement) != SQLITE_DONE) {
      status.status = bound ? "sqlite_insert_failed" : "sqlite_bind_failed";
      status.error = redact_sqlite_error(sqlite3_errmsg(db), sqlite_path);
      sqlite3_finalize(statement);
      std::string ignored;
      execute_sql(db, "ROLLBACK;", ignored);
      sqlite3_close(db);
      return status;
    }
  }
  sqlite3_finalize(statement);

  if (!execute_sql(db, "COMMIT;", sqlite_error)) {
    status.status = "sqlite_commit_failed";
    status.error = redact_sqlite_error(sqlite_error, sqlite_path);
    std::string ignored;
    execute_sql(db, "ROLLBACK;", ignored);
    sqlite3_close(db);
    return status;
  }
  const int close_status = sqlite3_close(db);
  status.written = true;
  status.record_count = plan.lineage.records.size();
  status.fallback = "sqlite";
  status.status = close_status == SQLITE_OK ? "ok" : "sqlite_close_failed";
  if (close_status != SQLITE_OK) {
    status.error = "SQLite evidence warehouse committed but close failed";
  }
  return status;
#else
  (void)plan;
  status.status = "sqlite_dependency_unavailable";
  return status;
#endif
}

}  // namespace inferlab_evidence_warehouse
