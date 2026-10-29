#include "disk_analysis/export.hpp"
#include "disk_analysis/format.hpp"

#include <ostream>
#include <string>

namespace disk_analysis {
namespace {

const char* type_name(NodeType type) {
    switch (type) {
    case NodeType::file: return "file";
    case NodeType::directory: return "directory";
    case NodeType::symlink: return "symlink";
    case NodeType::other: return "other";
    }
    return "other";
}

void write_node(std::ostream& output, const FileNode& node, std::size_t depth) {
    output << depth << ',' << type_name(node.type) << ',' << node.size << ','
           << node.file_count << ',' << node.directory_count << ','
           << (node.accessible ? "true" : "false") << ','
           << csv_escape(path_to_utf8(node.path)) << '\n';
    for (const auto& child : node.children) write_node(output, *child, depth + 1);
}

} // namespace

std::string csv_escape(std::string_view value) {
    bool quote = false;
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (char character : value) {
        if (character == '"') {
            escaped += "\"\"";
            quote = true;
        } else {
            if (character == ',' || character == '\n' || character == '\r') quote = true;
            escaped += character;
        }
    }
    if (!quote) return escaped;
    return '"' + escaped + '"';
}

void write_csv(std::ostream& output, const FileNode& root) {
    output << "depth,type,size_bytes,file_count,directory_count,accessible,path\n";
    write_node(output, root, 0);
}

} // namespace disk_analysis

