#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel)
cmake_bin=${CMAKE_BIN:-cmake}
ctest_bin=${CTEST_BIN:-ctest}
mkdir -p "${1:?Usage: core-check.sh OUTPUT}"
out=$(cd -- "$1" && pwd)

for version in aade95612ccf492819221b16ff896d11d138c907 3bea0580c10fb30943c405b230bdf3268519c6e2; do
    folder="$out/${version:0:7}"
    mkdir -p "$folder"
    git -C "$repo" archive "$version" CMakeLists.txt src tests | tar -x -C "$folder"
    "$cmake_bin" -S "$folder" -B "$folder/stock" -DCHOPPER_BUILD_VST3=OFF -DCMAKE_BUILD_TYPE=Release
    status=0
    "$cmake_bin" --build "$folder/stock" --parallel 4 >"$folder/stock.log" 2>&1 || status=$?
    cat "$folder/stock.log"
    printf 'Unmodified GCC build %s exit %s\n' "$version" "$status"
    if [[ $version == aade956* ]]; then
        test "$status" = 0
        "$ctest_bin" --test-dir "$folder/stock" --output-on-failure
    else
        test "$status" -ne 0
        grep -F -- '-Werror=misleading-indentation' "$folder/stock.log"
        "$ctest_bin" --test-dir "$folder/stock" -E editor_registry_reentrancy --output-on-failure
    fi
done

# Run the candidate registry assertions without making this warning fatal.
# This is a diagnostic build with a deliberate deviation from the CMake default.
"$cmake_bin" -S "$folder" -B "$folder/diagnostic" -DCHOPPER_BUILD_VST3=OFF \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-Wno-error=misleading-indentation
"$cmake_bin" --build "$folder/diagnostic" --parallel 4
"$ctest_bin" --test-dir "$folder/diagnostic" -V
