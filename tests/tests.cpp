#include "disk_analysis/analytics.hpp"
#include "disk_analysis/format.hpp"
#include "disk_analysis/scanner.hpp"
#include "disk_analysis/treemap.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; ++failures; } } while (false)
}

int main() {
    using namespace disk_analysis;
    CHECK(format_size(0) == "0 B");
    CHECK(format_size(1024) == "1.00 KiB");
    CHECK(parse_size("10MiB") == 10ULL * 1024 * 1024);
    CHECK(parse_size("1.5 GB") == static_cast<std::uintmax_t>(1.5 * 1024 * 1024 * 1024));
    CHECK(!parse_size("potato"));
    CHECK(json_escape("a\"b\n") == "a\\\"b\\n");

    const auto base = std::filesystem::temp_directory_path() / "disk-analysis-tests";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base / "nested");
    { std::ofstream file(base / "one.bin", std::ios::binary); file << std::string(100, 'a'); }
    { std::ofstream file(base / "nested" / "two.bin", std::ios::binary); file << std::string(200, 'b'); }
    Scanner scanner;
    auto result = scanner.scan(base);
    CHECK(result.root != nullptr);
    CHECK(result.root->size == 300);
    CHECK(result.root->file_count == 2);
    CHECK(result.root->directory_count == 2);
    const auto layout = layout_treemap(*result.root, 80, 20);
    CHECK(!layout.empty());
    for (const auto& item : layout) {
        CHECK(item.bounds.x >= 0 && item.bounds.y >= 0);
        CHECK(item.bounds.x + item.bounds.width <= 80);
        CHECK(item.bounds.y + item.bounds.height <= 20);
    }

    FileNode synthetic;
    synthetic.name = "root";
    synthetic.type = NodeType::directory;
    std::size_t item_index = 0;
    for (std::uintmax_t size : {400ULL, 250ULL, 150ULL, 100ULL, 60ULL, 40ULL}) {
        auto child = std::make_unique<FileNode>();
        child->name = "item-" + std::to_string(size);
        child->type = size == 400 ? NodeType::directory : NodeType::file;
        const std::string suffix = item_index++ % 2 == 0 ? ".TXT" : ".bin";
        child->path = std::filesystem::path(child->name + suffix);
        child->size = size;
        synthetic.size += size;
        synthetic.children.push_back(std::move(child));
    }
    const auto compact = layout_treemap(synthetic, 60, 20, {4, TreemapMode::all});
    CHECK(compact.size() == 4);
    CHECK(compact.back().aggregated);
    CHECK(compact.back().label == "Other");
    CHECK(compact.back().size == 200);
    for (std::size_t first = 0; first < compact.size(); ++first) {
        const auto& a = compact[first].bounds;
        CHECK(a.width > 0 && a.height > 0);
        for (std::size_t second = first + 1; second < compact.size(); ++second) {
            const auto& b = compact[second].bounds;
            const bool overlap = a.x < b.x + b.width && a.x + a.width > b.x &&
                                 a.y < b.y + b.height && a.y + a.height > b.y;
            CHECK(!overlap);
        }
    }
    const auto dirs = layout_treemap(synthetic, 60, 20, {30, TreemapMode::directories});
    CHECK(dirs.size() == 1);
    CHECK(dirs.front().node && dirs.front().node->type == NodeType::directory);

    const auto largest = largest_files(synthetic, 2);
    CHECK(largest.size() == 2);
    CHECK(largest[0]->size == 250);
    CHECK(largest[1]->size == 150);
    CHECK(largest_files(synthetic, 0).empty());

    const auto extension_statistics = analyze_extensions(synthetic, 10);
    CHECK(extension_statistics.size() == 2);
    CHECK(extension_statistics[0].extension == ".bin");
    CHECK(extension_statistics[0].size == 390);
    CHECK(extension_statistics[0].file_count == 3);
    CHECK(extension_statistics[1].extension == ".txt");
    CHECK(extension_statistics[1].size == 210);
    CHECK(extension_statistics[1].file_count == 2);

    ScanResult json_result;
    json_result.root = std::make_unique<FileNode>();
    json_result.root->name = synthetic.name;
    json_result.root->path = synthetic.path;
    json_result.root->type = synthetic.type;
    json_result.root->size = synthetic.size;
    for (const auto& source : synthetic.children) {
        auto copy = std::make_unique<FileNode>();
        copy->path = source->path;
        copy->name = source->name;
        copy->type = source->type;
        copy->size = source->size;
        json_result.root->children.push_back(std::move(copy));
    }
    std::ostringstream json;
    write_json(json, json_result, 1, 2, 2);
    CHECK(json.str().find("\"version\":\"0.3.0\"") != std::string::npos);
    CHECK(json.str().find("\"largestFiles\":[") != std::string::npos);
    CHECK(json.str().find("\"extensions\":[") != std::string::npos);
    std::filesystem::remove_all(base, ec);
    if (failures) { std::cerr << failures << " test(s) failed\n"; return 1; }
    std::cout << "All tests passed\n";
    return 0;
}

