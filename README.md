# DiskAnalysis

DiskAnalysis 0.4 is a dependency-free C++20 command-line disk usage analyzer with a squarified terminal treemap, file analytics, and verified duplicate detection.

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
```

Run `disk-analysis --help` for every option. Symlinks are not followed by default. Press `Ctrl+C` to stop a long scan safely and receive the partial result.

Version 0.4 can find duplicate files and estimate reclaimable space. Duplicate detection is opt-in because it reads candidate file contents. Candidates are grouped by size, hashed in streaming mode, and then compared byte-for-byte before being reported. No files are modified or deleted.

## Exit codes

- `0`: completed (individual unreadable entries may have been skipped)
- `1`: invalid arguments
- `2`: input path does not exist
- `3`: input path cannot be scanned
- `4`: cancelled with `Ctrl+C`

