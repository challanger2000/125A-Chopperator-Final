#!/usr/bin/env bash
set -euo pipefail

exec bash "$(dirname -- "${BASH_SOURCE[0]}")/build-probe.sh" "${1:?Usage: build-stress.sh WORK_DIRECTORY}" stress
