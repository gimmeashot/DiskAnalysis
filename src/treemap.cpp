#include "disk_analysis/treemap.hpp"
#include "disk_analysis/format.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ostream>
#include <string>

namespace disk_analysis {

std::vector<TreemapItem> layout_treemap(const FileNode& root, int width, int height) {
    std::vector<TreemapItem> result;
    if (width <= 0 || height <= 0 || root.size == 0) return result;
    std::vector<const FileNode*> nodes;
    for (const auto& child : root.children) if (child->size > 0) nodes.push_back(child.get());
    if (nodes.empty()) return result;
    std::sort(nodes.begin(), nodes.end(), [](auto* a, auto* b) { return a->size > b->size; });

    Rect remaining{0, 0, width, height};
    std::uintmax_t remaining_size{};
    for (auto* node : nodes) remaining_size += node->size;
    for (std::size_t i = 0; i < nodes.size() && remaining.width > 0 && remaining.height > 0; ++i) {
        const bool final = i + 1 == nodes.size();
        Rect bounds = remaining;
        if (remaining.width >= remaining.height) {
            int allocated = final ? remaining.width : static_cast<int>(std::llround(
                static_cast<double>(remaining.width) * nodes[i]->size / remaining_size));
            allocated = std::clamp(allocated, 1, remaining.width);
            bounds.width = allocated;
            remaining.x += allocated;
            remaining.width -= allocated;
        } else {
            int allocated = final ? remaining.height : static_cast<int>(std::llround(
                static_cast<double>(remaining.height) * nodes[i]->size / remaining_size));
            allocated = std::clamp(allocated, 1, remaining.height);
            bounds.height = allocated;
            remaining.y += allocated;
            remaining.height -= allocated;
        }
        result.push_back({nodes[i], bounds, 100.0 * nodes[i]->size / root.size});
        remaining_size -= nodes[i]->size;
    }
    return result;
}

static std::string fit(std::string value, int width) {
    if (width <= 0) return {};
    if (static_cast<int>(value.size()) > width) {
        if (width <= 3) return value.substr(0, static_cast<std::size_t>(width));
        value = value.substr(0, static_cast<std::size_t>(width - 3)) + "...";
    }
    value.resize(static_cast<std::size_t>(width), ' ');
    return value;
}

void render_treemap(std::ostream& out, const FileNode& root, int width, int height, bool color) {
    width = std::clamp(width, 20, 240);
    height = std::clamp(height, 4, 60);
    const auto items = layout_treemap(root, width, height);
    if (items.empty()) { out << "(no sized entries to display)\n"; return; }
    std::vector<std::string> canvas(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), ' '));
    std::vector<int> owners(static_cast<std::size_t>(width * height), -1);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& r = items[index].bounds;
        for (int y = r.y; y < r.y + r.height; ++y)
            for (int x = r.x; x < r.x + r.width; ++x) owners[static_cast<std::size_t>(y * width + x)] = static_cast<int>(index);
        if (r.width >= 2 && r.height >= 2) {
            for (int x = r.x; x < r.x + r.width; ++x) { canvas[r.y][x] = '-'; canvas[r.y + r.height - 1][x] = '-'; }
            for (int y = r.y; y < r.y + r.height; ++y) { canvas[y][r.x] = '|'; canvas[y][r.x + r.width - 1] = '|'; }
            canvas[r.y][r.x] = canvas[r.y][r.x + r.width - 1] = '+';
            canvas[r.y + r.height - 1][r.x] = canvas[r.y + r.height - 1][r.x + r.width - 1] = '+';
            if (r.width > 4 && r.height > 2) {
                const auto label = fit(items[index].node->name, r.width - 2);
                canvas[r.y + 1].replace(static_cast<std::size_t>(r.x + 1), label.size(), label);
            }
            if (r.width > 8 && r.height > 3) {
                const auto label = fit(format_size(items[index].node->size), r.width - 2);
                canvas[r.y + 2].replace(static_cast<std::size_t>(r.x + 1), label.size(), label);
            }
        }
    }
    static constexpr std::array<const char*, 6> colors = {"\x1b[44m", "\x1b[42m", "\x1b[45m", "\x1b[46m", "\x1b[43m", "\x1b[41m"};
    for (int y = 0; y < height; ++y) {
        int active = -2;
        for (int x = 0; x < width; ++x) {
            const int owner = owners[static_cast<std::size_t>(y * width + x)];
            if (color && owner != active) {
                if (owner >= 0) out << colors[static_cast<std::size_t>(owner) % colors.size()] << "\x1b[97m";
                else out << "\x1b[0m";
                active = owner;
            }
            out << canvas[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
        }
        if (color) out << "\x1b[0m";
        out << '\n';
    }
}

} // namespace disk_analysis

