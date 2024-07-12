# Based on https://github.com/krux02/minimal_cmake_example

cmake_minimum_required(VERSION 3.0)
project(crash_tutorial)

# set warning levels and select C++23 language standard
add_compile_options ( -std=c++23 -g -Og -Wall -Wfloat-conversion)

# Make the generated binary more suitable for analysing backtraces
add_compile_options (-rdynamic)
add_link_options(-no-pie)

# This allows to include files relative to the root of the src directory
include_directories(src/)

###############################################################################
## target definitions #########################################################
###############################################################################

file(GLOB_RECURSE example1_files src/example1/*)
add_executable(example1 ${example1_files})

file(GLOB_RECURSE example2_files src/example2/*)
add_executable(example2 ${example2_files})

file(GLOB_RECURSE example3_files src/example3/*)
add_executable(example3 ${example3_files})

file(GLOB_RECURSE example4_files src/example4/*)
add_executable(example4 ${example4_files})
