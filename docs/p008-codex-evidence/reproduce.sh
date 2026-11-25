#!/usr/bin/env bash
set -euo pipefail

evidence=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$evidence" rev-parse --show-toplevel)
work=${1:-$(mktemp -d)}
mkdir -p -- "$work"
work=$(cd -- "$work" && pwd)
printf 'Audit work directory: %s\n' "$work"
cd -- "$work"

cmake_cmd=${CMAKE:-cmake}
ctest_cmd=${CTEST:-ctest}
if [[ ! -d vst3sdk/.git ]]; then
  git clone --branch v3.8.1_build_84 --depth 1 --recurse-submodules --shallow-submodules \
    https://github.com/steinbergmedia/vst3sdk.git vst3sdk
fi
test "$(git -C vst3sdk rev-parse HEAD)" = 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
"$cmake_cmd" -S vst3sdk -B sdk-build -DCMAKE_BUILD_TYPE=Release \
  -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF
"$cmake_cmd" --build sdk-build --parallel 4 --target sdk sdk_hosting
"$cmake_cmd" -S "$repo" -B core-build -DCMAKE_BUILD_TYPE=Release -DCHOPPER_BUILD_VST3=OFF
"$cmake_cmd" --build core-build --parallel 4
"$ctest_cmd" --test-dir core-build -V | tee ctest.log

python3 -m venv qa-tools
qa-tools/bin/python -m pip install --only-binary=:all: imageio-ffmpeg==0.6.0 pillow==11.3.0
mkdir -p fixture-bin
ln -sf "$(qa-tools/bin/python -c 'import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())')" fixture-bin/ffmpeg
export PATH="$work/fixture-bin:$PATH" PYTHONDONTWRITEBYTECODE=1
qa-tools/bin/python "$repo/tools/prepare_native_gui.py" gui
qa-tools/bin/python "$repo/tools/prepare_programme_fixtures.py" programme
qa-tools/bin/python "$evidence/verify-assets.py"

run_probe() {
  local logfile=$1 expected=$2 actual=0
  shift 2
  "$@" > "$logfile" 2>&1 || actual=$?
  cat -- "$logfile"
  if [[ "$actual" != "$expected" ]]; then
    printf '%s: expected exit %s, got %s\n' "$logfile" "$expected" "$actual" >&2
    return 1
  fi
}
for probe in probe extra stress clock-meter; do
  bash "$evidence/build-$probe.sh" "$work"
done
run_probe probe.log 1 ./probe
run_probe negative.log 1 ./probe negative
run_probe clock-meter.log 1 ./clock-meter
run_probe extra.log 0 ./extra
run_probe stress.log 1 ./stress
run_probe stress-repeat.log 1 ./stress

# Recreate the two test-only copies rather than tracking an SDK-linked
# excerpt of Vst3.cpp or a second copy of the probe.
mkdir -p mutant
python3 - "$evidence" <<'PY'
from pathlib import Path
import sys

processor = Path("processor.inc").read_text()
interpolation = "previous+(target-previous)*fraction"
assert processor.count(interpolation) == 1
Path("mutant/processor.inc").write_text(processor.replace(interpolation, "previous+0*fraction"))
probe = (Path(sys.argv[1]) / "probe.cpp").read_text()
branch = 'if(command=="negative")automation(true);'
assert probe.count(branch) == 1
Path("mutant/probe.cpp").write_text(
    probe.replace(branch, 'if(command=="positive")automation(false);\n    else ' + branch)
)
PY
"${CXX:-c++}" -std=c++23 -O2 -DNDEBUG -pthread -I"$repo/src" -Ivst3sdk \
  mutant/probe.cpp vst3sdk/public.sdk/source/common/memorystream.cpp \
  -Lsdk-build/lib/Release -Wl,--start-group -lsdk_hosting -lsdk -lsdk_common -lbase -lpluginterfaces -Wl,--end-group -ldl \
  -o mutant/probe
run_probe mutant.log 1 mutant/probe positive
grep -Fx 'FAIL analytic MIX at 256 vs 4x64' mutant.log

mkdir -p asset-negative programme-negative
strip=125A_MixEngine_Analog_L_128px_100pct_128f.png
cp "gui/$strip" asset-negative/
printf x >> "asset-negative/$strip"
run_probe asset-negative.log 1 qa-tools/bin/python "$repo/tools/prepare_native_gui.py" asset-negative
grep -F "Asset mismatch $strip" asset-negative.log
test ! -e asset-negative/knob_L_0.5x.png
cp programme/guitar_e2.flac programme-negative/
printf x >> programme-negative/guitar_e2.flac
run_probe programme-negative.log 1 qa-tools/bin/python "$repo/tools/prepare_programme_fixtures.py" programme-negative
grep -F 'guitar_e2: source integrity mismatch' programme-negative.log
test ! -e programme-negative/guitar_e2.f64le
