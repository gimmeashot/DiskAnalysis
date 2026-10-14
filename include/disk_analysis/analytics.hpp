#pragma once

#include "disk_analysis/model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace disk_analysis {

struct ExtensionStatistics {
    std::string extension;
    std::uintmax_t size{};
    std::uint64_t file_count{};
};

std::vector<const FileNode*> largest_files(const FileNode& root, std::size_t limit);
std::vector<ExtensionStatistics> analyze_extensions(const FileNode& root, std::size_t limit);

} // namespace disk_analysis

