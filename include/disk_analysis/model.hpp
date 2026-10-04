#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace disk_analysis {

enum class NodeType { file, directory, symlink, other };

struct FileNode {
    std::filesystem::path path;
    std::string name;
    NodeType type{NodeType::other};
    std::uintmax_t size{};
    std::uint64_t file_count{};
    std::uint64_t directory_count{};
    bool accessible{true};
    std::vector<std::unique_ptr<FileNode>> children;
};

struct ScanError {
    std::filesystem::path path;
    std::string message;
};

struct ScanStatistics {
    std::uintmax_t total_size{};
    std::uint64_t files_scanned{};
    std::uint64_t directories_scanned{};
    std::uint64_t symlinks_skipped{};
    std::uint64_t skipped_items{};
    std::chrono::milliseconds elapsed{};
};

struct ScanResult {
    std::unique_ptr<FileNode> root;
    ScanStatistics statistics;
    std::vector<ScanError> errors;
    bool cancelled{};
};

} // namespace disk_analysis

