#!/bin/bash
set -euo pipefail
src=$(cd -- "$(dirname -- "$0")/../.." && pwd)
out=$(realpath -m -- "${1:?Usage: CROSS_COMPILE=/path/arm-eabi- build.sh OUTPUT [totoro|cooperve|tassve] [default|on|off]}")
device=${2:-totoro}
aoa=${3:-default}
case "$device" in totoro|cooperve|tassve) ;; *) echo 'Unknown board' >&2; exit 2 ;; esac
case "$aoa" in default|on|off) ;; *) echo 'Unknown AOA setting' >&2; exit 2 ;; esac
case "$out/" in "$src/"*) echo 'Use an output directory outside the source tree' >&2; exit 2 ;; esac
: "${CROSS_COMPILE:?Use the documented Android ARM EABI GCC 4.6 toolchain}"
case "$("${CROSS_COMPILE}gcc" -dumpversion)" in 4.6*) ;; *) echo 'GCC 4.6 is required' >&2; exit 2 ;; esac
mkdir -p "$out"
export KBUILD_BUILD_USER=${KBUILD_BUILD_USER:-bcm21553}
export KBUILD_BUILD_HOST=${KBUILD_BUILD_HOST:-port239}
export KBUILD_BUILD_VERSION=${KBUILD_BUILD_VERSION:-1}
export KBUILD_BUILD_TIMESTAMP=${KBUILD_BUILD_TIMESTAMP:-$(git -C "$src" show -s --format=%cD HEAD)}
args=(-C "$src" O="$out" ARCH=arm LOCALVERSION= CROSS_COMPILE="$CROSS_COMPILE" HOSTCC="${HOSTCC:-gcc-11}")
make "${args[@]}" "cyanogenmod_${device}_defconfig"
if [ "$aoa" != default ]; then
    action=--enable
    [ "$aoa" != off ] || action=--disable
    "$src/scripts/config" --file "$out/.config" "$action" USB_ANDROID_ACCESSORY
    make "${args[@]}" oldnoconfig
fi
make "${args[@]}" -j"${JOBS:-6}" zImage modules
python3 "$src/tools/port239/verify_build.py" --source "$src" --build "$out" --device "$device" --aoa "$aoa"
