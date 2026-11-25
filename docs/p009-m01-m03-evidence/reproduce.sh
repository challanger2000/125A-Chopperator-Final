#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel)
candidate=a276f0228acf58230ee5ca5c48267f062b5c79c1
mkdir -p "${1:?Usage: reproduce.sh OUTPUT [VST3_SDK]}"
out=$(cd -- "$1" && pwd)
source="$out/candidate"
mkdir -p "$source"
git -C "$repo" archive "$candidate" CMakeLists.txt src tests | tar -x -C "$source"
printf 'Candidate %s\n' "$candidate"

python3 "$here/extract.py" "$source/src/Editor.hpp" "$out/editor-methods.inc"
cxx=${CXX:-c++}
"$cxx" -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -I"$source/src" -I"$out" "$here/host-contract.cpp" -o "$out/host-contract"
"$out/host-contract"

cmake=${CMAKE_BIN:-cmake}
ctest=${CTEST_BIN:-ctest}
"$cmake" -S "$source" -B "$out/core" -G "Unix Makefiles" \
    -DCHOPPER_BUILD_VST3=OFF -DCMAKE_BUILD_TYPE=Release
"$cmake" --build "$out/core" --parallel 4
rg '^CXX_FLAGS = .* -Werror( |$)' "$out/core/CMakeFiles/chopper_registry_tests.dir/flags.make"
"$ctest" --test-dir "$out/core" -V

if [[ ${2:-} ]]; then
    sdk=$(cd -- "$2" && pwd)
    test "$(git -C "$sdk" rev-parse HEAD)" = 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
    "$cxx" -std=c++20 -Wno-multichar -DNDEBUG -fsyntax-only -I"$sdk" -I"$sdk/vstgui4" "$source/src/Vst3.cpp"
    printf 'VST3 translation-unit syntax checked with pinned SDK (Linux)\n'
fi
