#!/usr/bin/env bash
set -euo pipefail

evidence=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$evidence" rev-parse --show-toplevel)
work=$(cd -- "${1:?Usage: build-probe.sh WORK_DIRECTORY [PROBE]}" && pwd)
probe=${2:-probe}
case "$probe" in
  probe|extra|stress|clock-meter) ;;
  *) printf 'Unknown probe: %s\n' "$probe" >&2; exit 2 ;;
esac
sdk="$work/vst3sdk"
if [[ $(git -C "$sdk" rev-parse HEAD) != 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96 ]]; then
  printf 'Unexpected VST3 SDK revision\n' >&2
  exit 2
fi

sed -e '/^#include "Editor.hpp"$/d' -e '/^class Choice final/,$d' "$repo/src/Vst3.cpp" > "$work/processor.inc"
printf '\n}\n' >> "$work/processor.inc"

"${CXX:-c++}" -std=c++23 -O2 -DNDEBUG -pthread -I"$repo/src" -I"$work" -I"$sdk" \
  "$evidence/$probe.cpp" "$sdk/public.sdk/source/common/memorystream.cpp" \
  -L"$work/sdk-build/lib/Release" -Wl,--start-group -lsdk_hosting -lsdk -lsdk_common -lbase -lpluginterfaces -Wl,--end-group -ldl \
  -o "$work/$probe"
