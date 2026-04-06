#/usr/bin/env sh

set -e

cmake -S . -B build
cmake --build build
cd build && ./core_test
cd ..
