# DiskAnalysis

DiskAnalysis 0.5 is a standalone C++20 command-line disk usage analyzer with a squarified terminal treemap, file analytics, verified duplicate detection, and CSV export.

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
```

Run `disk-analysis --help` for every option. Symlinks are not followed by default. Press `Ctrl+C` to stop a long scan safely and receive the partial result.

Version 0.5 adds `--csv FILE`, which exports every retained tree node with its depth, type, byte size, aggregate counts, accessibility status, and path. Duplicate detection remains opt-in and never modifies or deletes files.

Prebuilt Windows releases contain a native x64 executable linked with the static MSVC runtime, so no separate C++ runtime installation is required.

## Windows executable

Download `DiskAnalysis-0.5.0-win-x64.exe` from the [v0.5.0 release](https://github.com/gimmeashot/DiskAnalysis/releases/tag/v0.5.0). The executable is 423,936 bytes and has SHA-256 `F018E3B017A97409F101962D9E5C18D07C55BB8500BD511E422283CCEB49611A`.

## Exit codes

- `0`: completed (individual unreadable entries may have been skipped)
- `1`: invalid arguments
- `2`: input path does not exist
- `3`: input path cannot be scanned
- `4`: cancelled with `Ctrl+C`
- `5`: output file could not be written or an internal output error occurred

