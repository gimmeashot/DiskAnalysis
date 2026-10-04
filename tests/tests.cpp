#include "disk_analysis/format.hpp"
#include "disk_analysis/scanner.hpp"
#include "disk_analysis/treemap.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
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
    std::filesystem::remove_all(base, ec);
    if (failures) { std::cerr << failures << " test(s) failed\n"; return 1; }
    std::cout << "All tests passed\n";
    return 0;
}

