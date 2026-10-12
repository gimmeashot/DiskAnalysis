#include "disk_analysis/format.hpp"
#include "disk_analysis/scanner.hpp"
#include "disk_analysis/treemap.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {
std::atomic_bool cancelled{false};
void on_signal(int) { cancelled.store(true); }

struct CliOptions {
    std::filesystem::path path{"."};
    disk_analysis::ScanOptions scan;
    std::size_t top{20};
    int width{};
    int height{16};
    disk_analysis::TreemapOptions treemap_options;
    enum class Color { automatic, always, never } color{Color::automatic};
    bool treemap{true};
    bool json{};
    bool quiet{};
    bool help{};
};

void help() {
    std::cout <<
        "DiskAnalysis 0.2.0 - console disk usage analyzer\n\n"
        "Usage: disk-analysis [scan] [path] [options]\n\n"
        "Options:\n"
        "  --depth N           Maximum scan depth\n"
        "  --top N             Number of largest entries (default: 20)\n"
        "  --min-size SIZE     Keep files at least this large (e.g. 10MiB)\n"
        "  --exclude TEXT      Skip paths containing TEXT (repeatable)\n"
        "  --follow-symlinks   Follow directory symlinks (use carefully)\n"
        "  --no-treemap        Do not draw a treemap\n"
        "  --width N           Treemap width (20..240)\n"
        "  --height N          Treemap height (4..60)\n"
        "  --treemap-items N   Maximum blocks, including Other (default: 30)\n"
        "  --treemap-mode MODE all, dirs, or files (default: all)\n"
        "  --color MODE        auto, always, or never\n"
        "  --json              Emit JSON instead of human-readable output\n"
        "  --quiet             Hide scan progress\n"
        "  --help              Show this help\n"
        "  --version           Show version\n";
}

bool parse_number(const std::string& value, std::size_t& target) {
    try { std::size_t used{}; target = std::stoull(value, &used); return used == value.size(); }
    catch (...) { return false; }
}

std::filesystem::path cli_path(const std::string& value) {
#ifdef _WIN32
    const auto* first = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path(std::u8string(first, first + value.size()));
#else
    return std::filesystem::path(value);
#endif
}

std::optional<CliOptions> parse_cli(int argc, char** argv) {
    CliOptions options;
    bool path_set = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "scan" && !path_set) continue;
        if (arg == "--help" || arg == "-h") { options.help = true; continue; }
        if (arg == "--version") { std::cout << "disk-analysis 0.2.0\n"; std::exit(0); }
        if (arg == "--follow-symlinks") { options.scan.follow_symlinks = true; continue; }
        if (arg == "--no-treemap") { options.treemap = false; continue; }
        if (arg == "--json") { options.json = true; options.quiet = true; continue; }
        if (arg == "--quiet") { options.quiet = true; continue; }
        auto need_value = [&](const char* name) -> std::optional<std::string> {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << name << '\n'; return std::nullopt; }
            return std::string(argv[++i]);
        };
        if (arg == "--depth" || arg == "--top" || arg == "--width" || arg == "--height" || arg == "--treemap-items") {
            auto value = need_value(arg.c_str()); if (!value) return std::nullopt;
            std::size_t number{}; if (!parse_number(*value, number)) { std::cerr << "Invalid number: " << *value << '\n'; return std::nullopt; }
            if (arg == "--depth") options.scan.max_depth = number;
            else if (arg == "--top") options.top = number;
            else if (arg == "--width") options.width = static_cast<int>(number);
            else if (arg == "--height") options.height = static_cast<int>(number);
            else options.treemap_options.max_items = number;
            continue;
        }
        if (arg == "--treemap-mode") {
            auto value = need_value("--treemap-mode"); if (!value) return std::nullopt;
            if (*value == "all") options.treemap_options.mode = disk_analysis::TreemapMode::all;
            else if (*value == "dirs" || *value == "directories") options.treemap_options.mode = disk_analysis::TreemapMode::directories;
            else if (*value == "files") options.treemap_options.mode = disk_analysis::TreemapMode::files;
            else { std::cerr << "Treemap mode must be all, dirs, or files\n"; return std::nullopt; }
            continue;
        }
        if (arg == "--min-size") {
            auto value = need_value("--min-size"); if (!value) return std::nullopt;
            auto parsed = disk_analysis::parse_size(*value); if (!parsed) { std::cerr << "Invalid size: " << *value << '\n'; return std::nullopt; }
            options.scan.min_size = *parsed; continue;
        }
        if (arg == "--exclude") { auto value = need_value("--exclude"); if (!value) return std::nullopt; options.scan.excludes.push_back(*value); continue; }
        if (arg == "--color") {
            auto value = need_value("--color"); if (!value) return std::nullopt;
            if (*value == "auto") options.color = CliOptions::Color::automatic;
            else if (*value == "always") options.color = CliOptions::Color::always;
            else if (*value == "never") options.color = CliOptions::Color::never;
            else { std::cerr << "Color must be auto, always, or never\n"; return std::nullopt; }
            continue;
        }
        if (!arg.empty() && arg[0] == '-') { std::cerr << "Unknown option: " << arg << '\n'; return std::nullopt; }
        if (path_set) { std::cerr << "Only one path can be scanned\n"; return std::nullopt; }
        options.path = cli_path(arg); path_set = true;
    }
    return options;
}

bool terminal_stdout() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

int terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) return info.srWindow.Right - info.srWindow.Left + 1;
#else
    winsize size{}; if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) return size.ws_col;
#endif
    return 100;
}
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    const auto options = parse_cli(argc, argv);
    if (!options) { std::cerr << "Try --help for usage.\n"; return 1; }
    if (options->help) { help(); return 0; }
    std::error_code ec;
    if (!std::filesystem::exists(options->path, ec)) { std::cerr << "Path not found: " << disk_analysis::path_to_utf8(options->path) << '\n'; return 2; }

    std::signal(SIGINT, on_signal);
    disk_analysis::Scanner scanner(options->scan);
    const auto progress = options->quiet ? disk_analysis::ProgressCallback{} : disk_analysis::ProgressCallback{
        [](const auto& stats, const auto& path) {
            std::cerr << "\rScanning: " << stats.files_scanned << " files, " << disk_analysis::format_size(stats.total_size)
                      << " | " << disk_analysis::path_to_utf8(path).substr(0, 60) << "          " << std::flush;
        }};
    auto result = scanner.scan(options->path, &cancelled, progress);
    if (!options->quiet) std::cerr << "\r" << std::string(100, ' ') << "\r";
    if (!result.root) { std::cerr << "Cannot scan path: " << disk_analysis::path_to_utf8(options->path) << '\n'; return 3; }
    if (options->json) { disk_analysis::write_json(std::cout, result); return result.cancelled ? 4 : 0; }

    std::cout << "Path:        " << disk_analysis::path_to_utf8(result.root->path) << '\n'
              << "Total size:  " << disk_analysis::format_size(result.root->size) << '\n'
              << "Files:       " << result.root->file_count << '\n'
              << "Directories: " << result.root->directory_count << '\n'
              << "Skipped:     " << result.statistics.skipped_items << '\n'
              << "Elapsed:     " << result.statistics.elapsed.count() << " ms\n\n";
    std::cout << "Largest entries:\n";
    const std::size_t count = std::min(options->top, result.root->children.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto& node = *result.root->children[i];
        const double percent = result.root->size ? 100.0 * node.size / result.root->size : 0.0;
        std::cout << "  " << disk_analysis::format_size(node.size) << "\t" << static_cast<int>(percent + .5) << "%\t" << node.name << '\n';
    }
    if (options->treemap) {
        std::cout << "\nTreemap:\n";
        const bool color = options->color == CliOptions::Color::always || (options->color == CliOptions::Color::automatic && terminal_stdout());
        disk_analysis::render_treemap(std::cout, *result.root,
                                      options->width ? options->width : terminal_width() - 1,
                                      options->height, color, options->treemap_options);
    }
    if (!result.errors.empty()) {
        std::cerr << "Warning: " << result.errors.size() << " item(s) could not be read.\n";
        for (std::size_t i = 0; i < std::min<std::size_t>(result.errors.size(), 5); ++i)
            std::cerr << "  " << disk_analysis::path_to_utf8(result.errors[i].path) << ": " << result.errors[i].message << '\n';
    }
    if (result.cancelled) { std::cerr << "Scan cancelled.\n"; return 4; }
    return 0;
}
