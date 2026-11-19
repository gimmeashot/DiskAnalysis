# DiskAnalysis

DiskAnalysis 0.7 is a standalone C++20 command-line disk usage analyzer with a squarified terminal treemap, wildcard exclusions, age and size analytics, verified duplicate detection, and CSV export.

## Build

```console
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

MSVC, GCC, and Clang are supported. The resulting executable is named `disk-analysis`.

## Usage

```console
disk-analysis scan "C:\Users\User\Downloads"
disk-analysis . --top 10 --min-size 1MiB --color always
disk-analysis /var --depth 3 --exclude cache --json
disk-analysis . --treemap-items 15 --treemap-mode dirs
disk-analysis . --largest-files 25 --extensions 15
disk-analysis . --duplicates 10 --duplicate-min-size 10MiB
disk-analysis . --csv disk-report.csv
disk-analysis . --oldest-files 20 --older-than 365
disk-analysis . --exclude-glob "*.tmp" --exclude-glob "cache/*"
```

Run `disk-analysis --help` for every option. Symlinks are not followed by default. Press `Ctrl+C` to stop a long scan safely and receive the partial result.

Version 0.7 adds repeatable wildcard exclusions through `--exclude-glob PATTERN`. Patterns without a slash match names at any depth; patterns containing a slash match paths relative to the scanned root. `*` matches any sequence and `?` matches one character. Matching is case-insensitive on Windows and case-sensitive on other platforms.

Prebuilt Windows releases contain a native x64 executable linked with the static MSVC runtime, so no separate C++ runtime installation is required.

## Windows executable

Download `DiskAnalysis-0.7.0-win-x64.exe` from the [v0.7.0 release](https://github.com/gimmeashot/DiskAnalysis/releases/tag/v0.7.0). The executable is 466,944 bytes and has SHA-256 `8C5EED76F01BA8B65F625B49C2255122DF07253017F763CE447BD5B847B68A7E`.

## Exit codes

- `0`: completed (individual unreadable entries may have been skipped)
- `1`: invalid arguments
- `2`: input path does not exist
- `3`: input path cannot be scanned
- `4`: cancelled with `Ctrl+C`
- `5`: output file could not be written or an internal output error occurred

