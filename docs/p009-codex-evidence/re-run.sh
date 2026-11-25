#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel 2>/dev/null || git -C "${CHOPPER_AUDIT_REPO:?Set CHOPPER_AUDIT_REPO}" rev-parse --show-toplevel)
sdk=$(cd -- "${1:?usage: re-run.sh VST3_SDK SDK_BUILD OUTPUT}" && pwd)
sdk_build=$(cd -- "${2:?SDK_BUILD required}" && pwd)
mkdir -p "${3:?OUTPUT required}"
out=$(cd -- "$3" && pwd)
[[ $(git -C "$sdk" rev-parse HEAD) == 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96 ]]
cxx=${CXX:-c++}
for version in 33d81e8a2a66c95999207819efeecfe9de313ede aade95612ccf492819221b16ff896d11d138c907; do
    src="$out/$version/src"
    mkdir -p "$src"
    git -C "$repo" archive "$version" src | tar -x -C "${src%/src}"
    sed -e '/^#include "Editor.hpp"$/d' -e '/^class Choice final/,$d' "$src/Vst3.cpp" > "$src/processor.inc"
    printf '\n}\n' >> "$src/processor.inc"
    "$cxx" -include bit -std=c++23 -O2 -DNDEBUG -pthread -I"$src" -I"$sdk" \
        "$here/re-audit.cpp" "$sdk/public.sdk/source/common/memorystream.cpp" \
        -L"$sdk_build/lib/Release" -Wl,--start-group -lsdk_hosting -lsdk -lsdk_common -lbase -lpluginterfaces -Wl,--end-group -ldl \
        -o "$src/re-audit"
    status=0
    "$src/re-audit" > "$out/re-audit-${version:0:7}.log" 2>&1 || status=$?
    cat "$out/re-audit-${version:0:7}.log"
    if [[ $version == aade956* ]]; then test "$status" = 0; else test "$status" = 1; fi
done
src="$out/aade95612ccf492819221b16ff896d11d138c907/src"
"$cxx" -std=c++20 -Wno-multichar -DNDEBUG -fsyntax-only -I"$sdk" -I"$sdk/vstgui4" "$src/Vst3.cpp"
sed -n '/^struct EditorPreferences {/,/^};/p' "$src/Editor.hpp" > "$out/preferences.inc"
"$cxx" -std=c++20 -DNDEBUG -O0 -I"$out" "$here/reentrant.cpp" -o "$out/reentrant"
"$out/reentrant" > "$out/reentrant.log"
status=0
"$out/reentrant" detach >> "$out/reentrant.log" 2>&1 || status=$?
cat "$out/reentrant.log"
test "$status" = 1
grep -Fx 'FAIL synchronous detach during notification: bad_function_call' "$out/reentrant.log"

# Re-run the P008 paced/audio-save stress on P009, omitting the old
# DBL_MAX acceptance fixture. C08 above supplies its corrected oracle.
archived="$here/../p008-codex-evidence"
cp "$archived/probe.cpp" "$out/probe.cpp"
sed 's/busTest();rateTests();concurrency();deadlines();/busTest();concurrency();deadlines();/' "$archived/stress.cpp" > "$out/stress.cpp"
"$cxx" -std=c++23 -O2 -DNDEBUG -pthread -I"$src" -I"$sdk" \
    "$out/stress.cpp" "$sdk/public.sdk/source/common/memorystream.cpp" \
    -L"$sdk_build/lib/Release" -Wl,--start-group -lsdk_hosting -lsdk -lsdk_common -lbase -lpluginterfaces -Wl,--end-group -ldl \
    -o "$out/stress"
status=0
"$out/stress" > "$out/stress-p009.log" 2>&1 || status=$?
cat "$out/stress-p009.log"
test "$status" = 0 || test "$status" = 1
