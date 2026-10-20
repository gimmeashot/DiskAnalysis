#include "disk_analysis/analytics.hpp"
#include "disk_analysis/format.hpp"

#include <algorithm>
#include <cctype>
#include <array>
#include <fstream>
#include <map>
#include <optional>
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

std::optional<std::uint64_t> hash_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::uint64_t hash = 14695981039346656037ULL;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            hash *= 1099511628211ULL;
        }
    }
    if (!input.eof()) return std::nullopt;
    return hash;
}

bool files_equal(const std::filesystem::path& left_path, const std::filesystem::path& right_path) {
    std::ifstream left(left_path, std::ios::binary);
    std::ifstream right(right_path, std::ios::binary);
    if (!left || !right) return false;
    std::array<char, 64 * 1024> left_buffer{}, right_buffer{};
    do {
        left.read(left_buffer.data(), static_cast<std::streamsize>(left_buffer.size()));
        right.read(right_buffer.data(), static_cast<std::streamsize>(right_buffer.size()));
        if (left.gcount() != right.gcount()) return false;
        if (!std::equal(left_buffer.begin(), left_buffer.begin() + left.gcount(), right_buffer.begin())) return false;
    } while (left.gcount() > 0);
    return left.eof() && right.eof();
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

std::vector<DuplicateGroup> find_duplicates(const FileNode& root, std::size_t limit,
                                            std::uintmax_t minimum_size) {
    if (limit == 0) return {};
    std::vector<const FileNode*> files;
    collect_files(root, files);
    std::map<std::uintmax_t, std::vector<const FileNode*>> by_size;
    for (const FileNode* file : files) {
        if (file->size >= minimum_size) by_size[file->size].push_back(file);
    }

    std::vector<DuplicateGroup> result;
    for (const auto& [size, same_size] : by_size) {
        if (same_size.size() < 2) continue;
        std::map<std::uint64_t, std::vector<const FileNode*>> by_hash;
        for (const FileNode* file : same_size) {
            if (const auto hash = hash_file(file->path)) by_hash[*hash].push_back(file);
        }
        for (const auto& [hash, same_hash] : by_hash) {
            (void)hash;
            if (same_hash.size() < 2) continue;
            std::vector<DuplicateGroup> exact_groups;
            for (const FileNode* file : same_hash) {
                bool matched = false;
                for (auto& group : exact_groups) {
                    if (files_equal(group.files.front()->path, file->path)) {
                        group.files.push_back(file);
                        matched = true;
                        break;
                    }
                }
                if (!matched) exact_groups.push_back({size, 0, {file}});
            }
            for (auto& group : exact_groups) {
                if (group.files.size() < 2) continue;
                group.reclaimable_size = group.file_size * (group.files.size() - 1);
                result.push_back(std::move(group));
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.reclaimable_size != right.reclaimable_size)
            return left.reclaimable_size > right.reclaimable_size;
        return path_to_utf8(left.files.front()->path) < path_to_utf8(right.files.front()->path);
    });
    if (result.size() > limit) result.resize(limit);
    return result;
}

} // namespace disk_analysis

