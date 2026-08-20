#include "edge_runtime_input_snapshot.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <sstream>
#include <system_error>

#if defined(__linux__) || defined(__ANDROID__) || defined(__unix__) || \
    defined(__APPLE__)
#include <sys/stat.h>
#define INFERLAB_EDGE_HAS_POSIX_STAT 1
#else
#define INFERLAB_EDGE_HAS_POSIX_STAT 0
#endif

namespace inferlab::edge_runtime_matrix {
namespace {

constexpr char kPosixSnapshotSource[] =
    "posix_stat_device_inode_size_mtime_ctime";
constexpr char kPosixSnapshotUnavailable[] = "posix_stat_unavailable";
constexpr char kFilesystemSnapshotSource[] = "filesystem_path_size_mtime";
constexpr char kFilesystemSnapshotUnavailable[] =
    "filesystem_metadata_unavailable";

#if INFERLAB_EDGE_HAS_POSIX_STAT

void append_posix_timestamps(const struct stat& metadata,
                             std::ostringstream* identity) {
#if defined(__APPLE__)
  *identity << ':' << metadata.st_mtimespec.tv_sec << ':'
            << metadata.st_mtimespec.tv_nsec << ':'
            << metadata.st_ctimespec.tv_sec << ':'
            << metadata.st_ctimespec.tv_nsec;
#else
  *identity << ':' << metadata.st_mtim.tv_sec << ':'
            << metadata.st_mtim.tv_nsec << ':' << metadata.st_ctim.tv_sec
            << ':' << metadata.st_ctim.tv_nsec;
#endif
}

EdgeRuntimeInputSnapshot capture_posix_snapshot(
    const std::filesystem::path& path) {
  EdgeRuntimeInputSnapshot snapshot;
  snapshot.metadata_source = kPosixSnapshotUnavailable;
  const auto& native_path = path.native();
  if (path.empty() || native_path.find('\0') != std::string::npos) {
    return snapshot;
  }

  struct stat metadata {};
  if (::stat(native_path.c_str(), &metadata) != 0 ||
      !S_ISREG(metadata.st_mode) || metadata.st_size < 0) {
    return snapshot;
  }
  const auto file_size = static_cast<std::uintmax_t>(metadata.st_size);
  if (file_size > std::numeric_limits<std::uint64_t>::max()) {
    return snapshot;
  }

  std::ostringstream identity;
  identity << static_cast<std::uintmax_t>(metadata.st_dev) << ':'
           << static_cast<std::uintmax_t>(metadata.st_ino) << ':'
           << file_size;
  append_posix_timestamps(metadata, &identity);
  snapshot.observed = true;
  snapshot.file_size_bytes = static_cast<std::uint64_t>(file_size);
  snapshot.metadata_source = kPosixSnapshotSource;
  snapshot.opaque_identity = identity.str();
  return snapshot;
}

#endif

#if !INFERLAB_EDGE_HAS_POSIX_STAT
EdgeRuntimeInputSnapshot capture_filesystem_snapshot(
    const std::filesystem::path& path) {
  EdgeRuntimeInputSnapshot snapshot;
  snapshot.metadata_source = kFilesystemSnapshotUnavailable;
  if (path.empty()) return snapshot;

  std::error_code error;
  const auto status = std::filesystem::status(path, error);
  if (error || !std::filesystem::is_regular_file(status)) return snapshot;
  const auto file_size = std::filesystem::file_size(path, error);
  if (error || file_size > std::numeric_limits<std::uint64_t>::max()) {
    return snapshot;
  }
  const auto write_time = std::filesystem::last_write_time(path, error);
  if (error) return snapshot;
  const auto identity = std::filesystem::weakly_canonical(path, error);
  if (error) return snapshot;

  const std::string normalized_path =
      identity.lexically_normal().generic_string();
  std::ostringstream opaque_identity;
  opaque_identity << normalized_path.size() << ':' << normalized_path << ':'
                  << file_size << ':'
                  << write_time.time_since_epoch().count();
  snapshot.observed = true;
  snapshot.file_size_bytes = static_cast<std::uint64_t>(file_size);
  snapshot.metadata_source = kFilesystemSnapshotSource;
  snapshot.opaque_identity = opaque_identity.str();
  return snapshot;
}
#endif

}  // namespace

EdgeRuntimeInputSnapshot capture_edge_runtime_input_snapshot(
    const std::filesystem::path& path) {
#if INFERLAB_EDGE_HAS_POSIX_STAT
  return capture_posix_snapshot(path);
#else
  return capture_filesystem_snapshot(path);
#endif
}

EdgeRuntimeInputSnapshotComparison compare_edge_runtime_input_snapshots(
    const EdgeRuntimeInputSnapshot& baseline,
    const EdgeRuntimeInputSnapshot& current) noexcept {
  if (!baseline.observed || !current.observed ||
      baseline.opaque_identity.empty() || current.opaque_identity.empty()) {
    return EdgeRuntimeInputSnapshotComparison::kUnavailable;
  }
  return baseline.metadata_source == current.metadata_source &&
                 baseline.file_size_bytes == current.file_size_bytes &&
                 baseline.opaque_identity == current.opaque_identity
             ? EdgeRuntimeInputSnapshotComparison::kStable
             : EdgeRuntimeInputSnapshotComparison::kChanged;
}

bool is_known_edge_runtime_input_snapshot_source(
    const std::string& source) noexcept {
  return source == kPosixSnapshotSource ||
         source == kPosixSnapshotUnavailable ||
         source == kFilesystemSnapshotSource ||
         source == kFilesystemSnapshotUnavailable;
}

bool is_observed_edge_runtime_input_snapshot_source(
    const std::string& source) noexcept {
  return source == kPosixSnapshotSource ||
         source == kFilesystemSnapshotSource;
}

}  // namespace inferlab::edge_runtime_matrix
