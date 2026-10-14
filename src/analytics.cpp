#include "disk_analysis/analytics.hpp"
#include "disk_analysis/format.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

namespace disk_analysis {
namespace {

void collect_files(const FileNode& node, std::vector<const FileNode*>& output) {
    if (node.type == NodeType::file) {
        output.push_back(&node);
        return;
    }
    for (const auto& child : node.children) collect_files(*child, output);
}

std::string normalized_extension(const FileNode& node) {
    std::string extension = path_to_utf8(node.path.extension());
    if (extension.empty()) return "[no extension]";
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

} // namespace

std::vector<const FileNode*> largest_files(const FileNode& root, std::size_t limit) {
    if (limit == 0) return {};
    std::vector<const FileNode*> files;
    collect_files(root, files);
    std::sort(files.begin(), files.end(), [](const FileNode* left, const FileNode* right) {
        if (left->size != right->size) return left->size > right->size;
        return path_to_utf8(left->path) < path_to_utf8(right->path);
    });
    if (files.size() > limit) files.resize(limit);
    return files;
}

std::vector<ExtensionStatistics> analyze_extensions(const FileNode& root, std::size_t limit) {
    if (limit == 0) return {};
    std::vector<const FileNode*> files;
    collect_files(root, files);
    std::map<std::string, ExtensionStatistics> grouped;
    for (const FileNode* file : files) {
        const std::string extension = normalized_extension(*file);
        auto& statistics = grouped[extension];
        statistics.extension = extension;
        statistics.size += file->size;
        ++statistics.file_count;
    }
    std::vector<ExtensionStatistics> result;
    result.reserve(grouped.size());
    for (auto& [extension, statistics] : grouped) {
        (void)extension;
        result.push_back(std::move(statistics));
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.size != right.size) return left.size > right.size;
        return left.extension < right.extension;
    });
    if (result.size() > limit) result.resize(limit);
    return result;
}

} // namespace disk_analysis

