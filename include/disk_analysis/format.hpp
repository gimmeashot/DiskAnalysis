#pragma once

#include "disk_analysis/model.hpp"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace disk_analysis {

std::string format_size(std::uintmax_t bytes);
std::optional<std::uintmax_t> parse_size(std::string_view text);
std::string path_to_utf8(const std::filesystem::path& path);
std::string json_escape(std::string_view value);
void write_json(std::ostream& out, const ScanResult& result, std::size_t max_depth = 2,
                std::size_t largest_file_count = 10, std::size_t extension_count = 10);

} // namespace disk_analysis

