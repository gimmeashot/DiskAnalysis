#include "disk_analysis/treemap.hpp"
#include "disk_analysis/format.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <ostream>
#include <string>

namespace disk_analysis {
namespace {
struct WeightedItem {
    const FileNode* node{};
    std::string label;
    std::uintmax_t size{};
    double area{};
    bool aggregated{};
};

double worst_ratio(const std::vector<WeightedItem>& row, double short_side) {
    if (row.empty() || short_side <= 0.0) return std::numeric_limits<double>::infinity();
    double sum = 0.0, smallest = std::numeric_limits<double>::max(), largest = 0.0;
    for (const auto& item : row) {
        sum += item.area;
        smallest = std::min(smallest, item.area);
        largest = std::max(largest, item.area);
    }
    if (smallest <= 0.0 || sum <= 0.0) return std::numeric_limits<double>::infinity();
    const double side_squared = short_side * short_side;
    const double sum_squared = sum * sum;
    return std::max(side_squared * largest / sum_squared, sum_squared / (side_squared * smallest));
}

void place_row(const std::vector<WeightedItem>& row, Rect& remaining,
               std::vector<TreemapItem>& output, std::uintmax_t root_size) {
    if (row.empty() || remaining.width <= 0 || remaining.height <= 0) return;
    double row_area = 0.0;
    for (const auto& item : row) row_area += item.area;
    const bool horizontal = remaining.width >= remaining.height;
    const int long_side = horizontal ? remaining.width : remaining.height;
    const int short_side = horizontal ? remaining.height : remaining.width;
    int thickness = static_cast<int>(std::llround(row_area / std::max(1, long_side)));
    thickness = std::clamp(thickness, 1, short_side);

    int cursor = horizontal ? remaining.x : remaining.y;
    int available = long_side;
    double area_left = row_area;
    for (std::size_t index = 0; index < row.size(); ++index) {
        const bool last = index + 1 == row.size();
        int length = last ? available : static_cast<int>(std::llround(
            available * row[index].area / std::max(area_left, 1e-12)));
        length = std::clamp(length, 1, available);
        const Rect bounds = horizontal ? Rect{cursor, remaining.y, length, thickness}
                                       : Rect{remaining.x, cursor, thickness, length};
        output.push_back({row[index].node, row[index].label, row[index].size, bounds,
                          root_size ? 100.0 * row[index].size / root_size : 0.0,
                          row[index].aggregated});
        cursor += length;
        available -= length;
        area_left -= row[index].area;
    }
    if (horizontal) { remaining.y += thickness; remaining.height -= thickness; }
    else { remaining.x += thickness; remaining.width -= thickness; }
}

std::string fit(std::string value, int width) {
    if (width <= 0) return {};
    if (static_cast<int>(value.size()) > width) {
        if (width <= 3) return value.substr(0, static_cast<std::size_t>(width));
        value = value.substr(0, static_cast<std::size_t>(width - 3)) + "...";
    }
    value.resize(static_cast<std::size_t>(width), ' ');
    return value;
}

bool visible_in_mode(const FileNode& node, TreemapMode mode) {
    if (mode == TreemapMode::directories) return node.type == NodeType::directory;
    if (mode == TreemapMode::files) return node.type == NodeType::file;
    return true;
}
} // namespace

std::vector<TreemapItem> layout_treemap(const FileNode& root, int width, int height,
                                        TreemapOptions options) {
    std::vector<TreemapItem> result;
    if (width <= 0 || height <= 0 || root.size == 0 || options.max_items == 0) return result;
    std::vector<WeightedItem> items;
    for (const auto& child : root.children) {
        if (child->size > 0 && visible_in_mode(*child, options.mode))
            items.push_back({child.get(), child->name, child->size, 0.0, false});
    }
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.size > b.size; });
    if (items.empty()) return result;
    if (items.size() > options.max_items) {
        const std::size_t keep = options.max_items > 1 ? options.max_items - 1 : 0;
        std::uintmax_t other_size{};
        for (std::size_t index = keep; index < items.size(); ++index) other_size += items[index].size;
        items.resize(keep);
        items.push_back({nullptr, "Other", other_size, 0.0, true});
    }
    std::uintmax_t displayed_size{};
    for (const auto& item : items) displayed_size += item.size;
    const double total_area = static_cast<double>(width) * height;
    for (auto& item : items) item.area = total_area * item.size / displayed_size;

    Rect remaining{0, 0, width, height};
    std::vector<WeightedItem> row;
    std::size_t next = 0;
    while (next < items.size() && remaining.width > 0 && remaining.height > 0) {
        const double short_side = static_cast<double>(std::min(remaining.width, remaining.height));
        auto candidate = row;
        candidate.push_back(items[next]);
        if (row.empty() || worst_ratio(candidate, short_side) <= worst_ratio(row, short_side)) {
            row.push_back(items[next++]);
        } else {
            place_row(row, remaining, result, root.size);
            row.clear();
        }
    }
    if (!row.empty()) place_row(row, remaining, result, root.size);
    return result;
}

void render_treemap(std::ostream& out, const FileNode& root, int width, int height, bool color,
                    TreemapOptions options) {
    width = std::clamp(width, 20, 240);
    height = std::clamp(height, 4, 60);
    const auto items = layout_treemap(root, width, height, options);
    if (items.empty()) { out << "(no sized entries to display)\n"; return; }
    std::vector<std::string> canvas(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), ' '));
    std::vector<int> owners(static_cast<std::size_t>(width * height), -1);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& item = items[index];
        const auto& r = item.bounds;
        for (int y = r.y; y < r.y + r.height; ++y)
            for (int x = r.x; x < r.x + r.width; ++x) owners[static_cast<std::size_t>(y * width + x)] = static_cast<int>(index);
        if (r.width >= 2 && r.height >= 2) {
            for (int x = r.x; x < r.x + r.width; ++x) { canvas[r.y][x] = '-'; canvas[r.y + r.height - 1][x] = '-'; }
            for (int y = r.y; y < r.y + r.height; ++y) { canvas[y][r.x] = '|'; canvas[y][r.x + r.width - 1] = '|'; }
            canvas[r.y][r.x] = canvas[r.y][r.x + r.width - 1] = '+';
            canvas[r.y + r.height - 1][r.x] = canvas[r.y + r.height - 1][r.x + r.width - 1] = '+';
            if (r.width > 4 && r.height > 2) {
                const auto label = fit(item.label, r.width - 2);
                canvas[r.y + 1].replace(static_cast<std::size_t>(r.x + 1), label.size(), label);
            }
            if (r.width > 8 && r.height > 3) {
                const auto label = fit(format_size(item.size), r.width - 2);
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
