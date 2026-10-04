#include "disk_analysis/scanner.hpp"
#include "disk_analysis/format.hpp"

#include <algorithm>

namespace disk_analysis {

Scanner::Scanner(ScanOptions options) : options_(std::move(options)) {}

ScanResult Scanner::scan(const std::filesystem::path& root, std::atomic_bool* cancel, ProgressCallback progress) {
    result_ = {};
    cancel_ = cancel;
    progress_ = std::move(progress);
    visited_directories_.clear();
    last_progress_ = std::chrono::steady_clock::now();
    const auto started = last_progress_;
    result_.root = scan_node(root, 0);
    result_.cancelled = cancel_ && cancel_->load();
    result_.statistics.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    if (result_.root) result_.statistics.total_size = result_.root->size;
    return std::move(result_);
}

bool Scanner::excluded(const std::filesystem::path& path) const {
    const std::string value = path_to_utf8(path);
    return std::any_of(options_.excludes.begin(), options_.excludes.end(), [&](const std::string& pattern) {
        return value.find(pattern) != std::string::npos;
    });
}

void Scanner::report_error(const std::filesystem::path& path, const std::error_code& error) {
    ++result_.statistics.skipped_items;
    result_.errors.push_back({path, error.message()});
}

void Scanner::maybe_report_progress(const std::filesystem::path& path) {
    if (!progress_) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - last_progress_ >= std::chrono::milliseconds(150)) {
        progress_(result_.statistics, path);
        last_progress_ = now;
    }
}

std::unique_ptr<FileNode> Scanner::scan_node(const std::filesystem::path& path, std::size_t depth) {
    if ((cancel_ && cancel_->load()) || excluded(path)) return nullptr;
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec) { report_error(path, ec); return nullptr; }

    auto node = std::make_unique<FileNode>();
    node->path = path;
    node->name = path.filename().empty() ? path_to_utf8(path) : path_to_utf8(path.filename());

    if (std::filesystem::is_symlink(status)) {
        node->type = NodeType::symlink;
        if (!options_.follow_symlinks) { ++result_.statistics.symlinks_skipped; return node; }
        const auto target_status = std::filesystem::status(path, ec);
        if (ec) { report_error(path, ec); node->accessible = false; return node; }
        if (std::filesystem::is_directory(target_status)) node->type = NodeType::directory;
        else if (std::filesystem::is_regular_file(target_status)) node->type = NodeType::file;
    } else if (std::filesystem::is_directory(status)) node->type = NodeType::directory;
    else if (std::filesystem::is_regular_file(status)) node->type = NodeType::file;
    else node->type = NodeType::other;

    if (node->type == NodeType::file) {
        node->size = std::filesystem::file_size(path, ec);
        if (ec) { report_error(path, ec); node->accessible = false; node->size = 0; }
        node->file_count = 1;
        ++result_.statistics.files_scanned;
        result_.statistics.total_size += node->size;
        maybe_report_progress(path);
        return node;
    }
    if (node->type != NodeType::directory) return node;

    if (options_.follow_symlinks) {
        const auto canonical = std::filesystem::weakly_canonical(path, ec);
        if (ec) { report_error(path, ec); node->accessible = false; return node; }
        if (!visited_directories_.insert(path_to_utf8(canonical)).second) return node;
    }

    ++result_.statistics.directories_scanned;
    node->directory_count = 1;
    if (options_.max_depth && depth >= *options_.max_depth) return node;

    std::filesystem::directory_iterator iterator(path, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) { report_error(path, ec); node->accessible = false; return node; }
    for (const auto& entry : iterator) {
        if (cancel_ && cancel_->load()) break;
        auto child = scan_node(entry.path(), depth + 1);
        if (!child) continue;
        node->size += child->size;
        node->file_count += child->file_count;
        node->directory_count += child->directory_count;
        if (child->size >= options_.min_size || child->type == NodeType::directory) node->children.push_back(std::move(child));
    }
    std::sort(node->children.begin(), node->children.end(), [](const auto& a, const auto& b) { return a->size > b->size; });
    maybe_report_progress(path);
    return node;
}

} // namespace disk_analysis
