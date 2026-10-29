#pragma once

#include "disk_analysis/model.hpp"

#include <iosfwd>
#include <string_view>

namespace disk_analysis {

std::string csv_escape(std::string_view value);
void write_csv(std::ostream& output, const FileNode& root);

} // namespace disk_analysis

