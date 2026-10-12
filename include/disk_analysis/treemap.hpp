#pragma once

#include "disk_analysis/model.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace disk_analysis {

struct Rect { int x{}, y{}, width{}, height{}; };
enum class TreemapMode { all, directories, files };

struct TreemapOptions {
    std::size_t max_items{30};
    TreemapMode mode{TreemapMode::all};
};

struct TreemapItem {
    const FileNode* node{};
    std::string label;
    std::uintmax_t size{};
    Rect bounds{};
    double percentage{};
    bool aggregated{};
};

std::vector<TreemapItem> layout_treemap(const FileNode& root, int width, int height,
                                        TreemapOptions options = {});
void render_treemap(std::ostream& out, const FileNode& root, int width, int height, bool color,
                    TreemapOptions options = {});

} // namespace disk_analysis

