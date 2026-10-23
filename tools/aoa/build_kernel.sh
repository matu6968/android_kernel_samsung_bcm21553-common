#!/bin/bash
set -euo pipefail
repo_dir=$(cd -- "$(dirname -- "$0")/../.." && pwd)
exec "$repo_dir/tools/port239/build.sh" "${1:?Usage: CROSS_COMPILE=/path/arm-eabi- build_kernel.sh OUTPUT [on|off]}" totoro "${2:-on}"
