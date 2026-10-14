# DiskAnalysis

DiskAnalysis 0.3 is a dependency-free C++20 command-line disk usage analyzer with a squarified terminal treemap, largest-file search, and extension statistics.

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
```

Run `disk-analysis --help` for every option. Symlinks are not followed by default. Press `Ctrl+C` to stop a long scan safely and receive the partial result.

Version 0.3 finds the largest files anywhere in the scanned tree and summarizes disk usage by normalized file extension. Pass `0` to `--largest-files` or `--extensions` to hide either report.

## Exit codes

- `0`: completed (individual unreadable entries may have been skipped)
- `1`: invalid arguments
- `2`: input path does not exist
- `3`: input path cannot be scanned
- `4`: cancelled with `Ctrl+C`

