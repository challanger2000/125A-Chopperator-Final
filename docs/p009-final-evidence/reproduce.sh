#!/usr/bin/env bash
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel 2>/dev/null || git -C "${CHOPPER_AUDIT_REPO:?Set CHOPPER_AUDIT_REPO}" rev-parse --show-toplevel)
sdk=$(cd -- "${1:?Usage: reproduce.sh SDK SDK_BUILD OUTPUT}" && pwd)
sdk_build=$(cd -- "${2:?SDK_BUILD required}" && pwd)
mkdir -p "${3:?OUTPUT required}"
out=$(cd -- "$3" && pwd)
test "$(git -C "$sdk" rev-parse HEAD)" = 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
cxx=${CXX:-c++}
version=3bea0580c10fb30943c405b230bdf3268519c6e2
git -C "$repo" archive "$version" src tests/NativeVst3Tests.cpp | tar -x -C "$out"
src="$out/src"
sed -e '/^#include "Editor.hpp"$/d' -e '/^class Choice final/,$d' "$src/Vst3.cpp" > "$out/processor.inc"
printf '\n}\n' >> "$out/processor.inc"
sed -n '/^struct PacedOracle {/,/^};/p' "$out/tests/NativeVst3Tests.cpp" > "$out/paced-oracle.inc"
build_probe() {
    "$cxx" -std=c++23 -O2 -DNDEBUG -pthread -I"$src" -I"$out" -I"$sdk" "$1" \
        "$sdk/public.sdk/source/common/memorystream.cpp" -L"$sdk_build/lib/Release" \
        -Wl,--start-group -lsdk_hosting -lsdk -lsdk_common -lbase -lpluginterfaces -Wl,--end-group -ldl \
        -o "$2"
}
build_probe "$here/../p009-codex-evidence/re-audit.cpp" "$out/processor-audit"
"$out/processor-audit" | tee "$out/processor.log"
build_probe "$here/paced.cpp" "$out/paced"
"$out/paced" | tee "$out/paced.log"
cp "$here/recall-view.hpp" "$out/recall-view.hpp"
sed -e 's/^#include "Editor.hpp"$/#include "recall-view.hpp"/' -e '/^BEGIN_FACTORY/,$d' \
    "$src/Vst3.cpp" > "$out/controller.inc"
build_probe "$here/recall.cpp" "$out/recall"
"$out/recall" | tee "$out/recall.log"

"$cxx" -std=c++20 -DNDEBUG -Wno-multichar -fsyntax-only -I"$sdk" -I"$sdk/vstgui4" "$src/Vst3.cpp"

printf 'namespace chopper {\n' > "$out/preferences.hpp"
git -C "$repo" show aade956:src/Editor.hpp | sed -n '/^struct EditorPreferences {/,/^};/p' >> "$out/preferences.hpp"
printf '}\n' >> "$out/preferences.hpp"
"$cxx" -std=c++20 -O0 -include algorithm -include functional -include vector -I"$out" "$here/registry.cpp" -o "$out/old-registry"
status=0
"$out/old-registry" > "$out/registry.log" || status=$?
test "$status" = 1
grep -Fx 'N01 reproduced: bad_function_call' "$out/registry.log"
printf '#include "EditorPreferences.hpp"\n' > "$out/preferences.hpp"
"$cxx" -std=c++20 -O0 -I"$src" -I"$out" "$here/registry.cpp" -o "$out/current-registry"
"$out/current-registry" | tee -a "$out/registry.log"

# Read the actual predicate and request calculation, leaving the native
# geometry callback and VSTGUI SDK absent from this arithmetic probe.
git -C "$repo" show aade956:src/Editor.hpp > "$out/old-editor.hpp"
python3 - "$src/Editor.hpp" "$out/old-editor.hpp" "$out" <<'PY'
from pathlib import Path
import sys
out=Path(sys.argv[3])
text=Path(sys.argv[1]).read_text()
for source, target in ((text, "dpi-guard.inc"),
                       (Path(sys.argv[2]).read_text(), "old-dpi-guard.inc")):
    start=source.index("        const auto scale=getContentScaleFactor();",source.index("PLUGIN_API onSize("))
    end=source.index("        if(std::getenv",start)
    guard=source[start:end].replace("return Steinberg::kResultFalse;", "return false;")
    (out/target).write_text(guard)
start=text.index("        if(syncing||",text.index("    void syncPreferences(){"))
end=text.index("        syncing=false;",start)+len("        syncing=false;")
sync=text[start:end].replace("preferences.zoom","desired").replace("VSTGUI::CPoint(", "Point(")
(out/"dpi-sync.inc").write_text("void sync(){\n"+sync+"\n}\n")
PY
"$cxx" -std=c++20 -O2 -I"$out" "$here/dpi.cpp" -o "$out/dpi"
"$out/dpi" | tee "$out/dpi.log"

# Repeat the original concurrency and paced deadline probes. The updated
# C08 rate oracle is already exercised by processor-audit above.
archived="$here/../p008-codex-evidence"
cp "$archived/probe.cpp" "$out/probe.cpp"
sed 's/busTest();rateTests();concurrency();deadlines();/busTest();concurrency();deadlines();/' \
    "$archived/stress.cpp" > "$out/stress.cpp"
build_probe "$out/stress.cpp" "$out/stress"
status=0
"$out/stress" > "$out/stress.log" 2>&1 || status=$?
cat "$out/stress.log"
printf 'Stress exit status %s\n' "$status"
test "$status" = 0 || test "$status" = 1
