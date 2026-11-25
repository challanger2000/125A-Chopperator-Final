#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel)
prior="$here/../p009-m01-m03-evidence"
candidate=8859e8be9a3aa12a9f5439780d369e273a820e79
mkdir -p "${1:?Usage: reproduce.sh OUTPUT}"
out=$(cd -- "$1" && pwd)
git -C "$repo" archive "$candidate" src/Editor.hpp src/EditorGeometry.hpp src/EditorPreferences.hpp | tar -x -C "$out"
printf 'Source %s\n' "$candidate"
python3 "$prior/extract.py" "$out/src/Editor.hpp" "$out/editor-methods.inc"
# Reuse the previous fixture types, without its test functions or main.
# Count fallback SDK resize calls and remove the obsolete product flag.
sed '/^void recallEndpoints()/,$d
s/,deferredClose=false//
s/virtual bool beforeSizeChange(const CRect\&,const CRect\&){return true;}/virtual bool beforeSizeChange(const CRect\&,const CRect\&){++baseSizeChanges;return true;}/
s/int requests=0,closeCalls=0;/int requests=0,closeCalls=0,baseSizeChanges=0;/' \
    "$prior/host-contract.cpp" > "$out/host-model.inc"
"${CXX:-c++}" -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -I"$out/src" -I"$out" "$here/followup.cpp" -o "$out/followup"
"$out/followup"
