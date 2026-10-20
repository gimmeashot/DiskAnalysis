#include "disk_analysis/analytics.hpp"
#include "disk_analysis/format.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>

namespace disk_analysis {

std::string format_size(std::uintmax_t bytes) {
    static constexpr const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(units)) { value /= 1024.0; ++unit; }
    std::ostringstream out;
    if (unit == 0) out << bytes;
    else out << std::fixed << std::setprecision(value >= 100.0 ? 0 : value >= 10.0 ? 1 : 2) << value;
    return out.str() + " " + units[unit];
}

std::optional<std::uintmax_t> parse_size(std::string_view text) {
    std::string clean;
    for (char c : text) if (!std::isspace(static_cast<unsigned char>(c))) clean.push_back(c);
    std::size_t split = 0;
    while (split < clean.size() && (std::isdigit(static_cast<unsigned char>(clean[split])) || clean[split] == '.')) ++split;
    if (split == 0) return std::nullopt;
    double number{};
    try { number = std::stod(clean.substr(0, split)); } catch (...) { return std::nullopt; }
    if (number < 0) return std::nullopt;
    std::string suffix = clean.substr(split);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (!suffix.empty() && suffix.back() == 'B') suffix.pop_back();
    if (!suffix.empty() && suffix.back() == 'I') suffix.pop_back();
    std::uintmax_t multiplier = 1;
    if (suffix == "K") multiplier = 1024ULL;
    else if (suffix == "M") multiplier = 1024ULL * 1024;
    else if (suffix == "G") multiplier = 1024ULL * 1024 * 1024;
    else if (suffix == "T") multiplier = 1024ULL * 1024 * 1024 * 1024;
    else if (!suffix.empty()) return std::nullopt;
    const long double result = static_cast<long double>(number) * multiplier;
    if (result > std::numeric_limits<std::uintmax_t>::max()) return std::nullopt;
    return static_cast<std::uintmax_t>(result);
}

std::string path_to_utf8(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
#else
    return path.string();
#endif
}

std::string json_escape(std::string_view value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break; case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break; case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break; case '\r': out << "\\r"; break; case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
            else out << static_cast<char>(c);
        }
    }
    return out.str();
}

static void write_node(std::ostream& out, const FileNode& node, std::size_t depth, std::size_t max_depth) {
    out << "{\"name\":\"" << json_escape(node.name) << "\",\"path\":\""
        << json_escape(path_to_utf8(node.path)) << "\",\"size\":" << node.size
        << ",\"files\":" << node.file_count << ",\"directories\":" << node.directory_count;
    if (depth < max_depth && !node.children.empty()) {
        out << ",\"children\":[";
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            if (i) out << ',';
            write_node(out, *node.children[i], depth + 1, max_depth);
        }
        out << ']';
    }
    out << '}';
}

void write_json(std::ostream& out, const ScanResult& result, std::size_t max_depth,
                std::size_t largest_file_count, std::size_t extension_count,
                std::size_t duplicate_count, std::uintmax_t duplicate_min_size) {
    out << "{\"version\":\"0.4.0\",\"cancelled\":" << (result.cancelled ? "true" : "false")
        << ",\"elapsedMs\":" << result.statistics.elapsed.count() << ",\"skipped\":"
        << result.statistics.skipped_items << ",\"root\":";
    if (result.root) write_node(out, *result.root, 0, max_depth); else out << "null";
    out << ",\"largestFiles\":[";
    if (result.root) {
        const auto files = largest_files(*result.root, largest_file_count);
        for (std::size_t index = 0; index < files.size(); ++index) {
            if (index) out << ',';
            out << "{\"path\":\"" << json_escape(path_to_utf8(files[index]->path))
                << "\",\"size\":" << files[index]->size << '}';
        }
    }
    out << "],\"extensions\":[";
    if (result.root) {
        const auto extensions = analyze_extensions(*result.root, extension_count);
        for (std::size_t index = 0; index < extensions.size(); ++index) {
            if (index) out << ',';
            out << "{\"extension\":\"" << json_escape(extensions[index].extension)
                << "\",\"size\":" << extensions[index].size
                << ",\"files\":" << extensions[index].file_count << '}';
        }
    }
    out << "],\"duplicateGroups\":[";
    if (result.root) {
        const auto duplicates = find_duplicates(*result.root, duplicate_count, duplicate_min_size);
        for (std::size_t index = 0; index < duplicates.size(); ++index) {
            if (index) out << ',';
            out << "{\"fileSize\":" << duplicates[index].file_size
                << ",\"reclaimableSize\":" << duplicates[index].reclaimable_size
                << ",\"files\":[";
            for (std::size_t file_index = 0; file_index < duplicates[index].files.size(); ++file_index) {
                if (file_index) out << ',';
                out << '\"' << json_escape(path_to_utf8(duplicates[index].files[file_index]->path)) << '\"';
            }
            out << "]}";
        }
    }
    out << "]}\n";
}

} // namespace disk_analysis

