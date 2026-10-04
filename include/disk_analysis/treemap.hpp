#pragma once

#include "disk_analysis/model.hpp"

#include <iosfwd>
#include <vector>

namespace disk_analysis {

struct Rect { int x{}, y{}, width{}, height{}; };
struct TreemapItem { const FileNode* node{}; Rect bounds{}; double percentage{}; };

std::vector<TreemapItem> layout_treemap(const FileNode& root, int width, int height);
void render_treemap(std::ostream& out, const FileNode& root, int width, int height, bool color);

} // namespace disk_analysis

