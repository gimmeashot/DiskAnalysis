cmake_minimum_required(VERSION 3.20)
project(DiskAnalysis VERSION 0.4.0 LANGUAGES CXX)

option(DISK_ANALYSIS_BUILD_TESTS "Build tests" ON)

add_library(disk_analysis_lib
    src/analytics.cpp
    src/scanner.cpp
    src/treemap.cpp
    src/format.cpp
)
target_include_directories(disk_analysis_lib PUBLIC include)
target_compile_features(disk_analysis_lib PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(disk_analysis_lib PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(disk_analysis_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(disk-analysis src/main.cpp)
target_link_libraries(disk-analysis PRIVATE disk_analysis_lib)
if(MSVC)
    target_compile_options(disk-analysis PRIVATE /W4 /permissive- /utf-8)
endif()

include(CTest)
if(BUILD_TESTING AND DISK_ANALYSIS_BUILD_TESTS)
    add_executable(disk-analysis-tests tests/tests.cpp)
    target_link_libraries(disk-analysis-tests PRIVATE disk_analysis_lib)
    add_test(NAME disk-analysis-tests COMMAND disk-analysis-tests)
endif()

install(TARGETS disk-analysis RUNTIME DESTINATION bin)

