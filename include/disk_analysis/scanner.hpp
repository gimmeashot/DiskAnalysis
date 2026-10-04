#pragma once

#include "disk_analysis/model.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace disk_analysis {

struct ScanOptions {
    std::optional<std::size_t> max_depth;
    std::uintmax_t min_size{};
    bool follow_symlinks{};
    std::vector<std::string> excludes;
};

using ProgressCallback = std::function<void(const ScanStatistics&, const std::filesystem::path&)>;

class Scanner {
public:
    explicit Scanner(ScanOptions options = {});
    ScanResult scan(const std::filesystem::path& root, std::atomic_bool* cancel = nullptr,
                    ProgressCallback progress = {});

private:
    std::unique_ptr<FileNode> scan_node(const std::filesystem::path& path, std::size_t depth);
    bool excluded(const std::filesystem::path& path) const;
    void report_error(const std::filesystem::path& path, const std::error_code& error);
    void maybe_report_progress(const std::filesystem::path& path);

    ScanOptions options_;
    ScanResult result_;
    std::atomic_bool* cancel_{};
    ProgressCallback progress_;
    std::chrono::steady_clock::time_point last_progress_{};
    std::unordered_set<std::string> visited_directories_;
};

} // namespace disk_analysis
