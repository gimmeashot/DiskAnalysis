#pragma once

#include "disk_analysis/model.hpp"

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace disk_analysis {

struct ExtensionStatistics {
    std::string extension;
    std::uintmax_t size{};
    std::uint64_t file_count{};
};

struct DuplicateGroup {
    std::uintmax_t file_size{};
    std::uintmax_t reclaimable_size{};
    std::vector<const FileNode*> files;
};

std::vector<const FileNode*> largest_files(const FileNode& root, std::size_t limit);
std::vector<const FileNode*> oldest_files(const FileNode& root, std::size_t limit,
                                          std::optional<std::int64_t> modified_before = std::nullopt);
std::vector<ExtensionStatistics> analyze_extensions(const FileNode& root, std::size_t limit);
std::vector<DuplicateGroup> find_duplicates(const FileNode& root, std::size_t limit,
                                            std::uintmax_t minimum_size = 1);

} // namespace disk_analysis

