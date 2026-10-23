#!/system/bin/sh
# Run from the delivered CM9/totoro bundle; --verify performs no device writes.
set -eu
mode=${1:---verify}
case "$mode" in --verify|--install|--restore) ;; *) echo 'Use --verify, --install, or --restore' >&2; exit 2 ;; esac
task_busybox=${PORT239_BUSYBOX:-/sbin/busybox}
[ -x "$task_busybox" ] || task_busybox=$(command -v busybox)
bb() { "$task_busybox" "$@"; }
case "$0" in */*) bundle=${0%/*} ;; *) bundle=. ;; esac
cd "$bundle"
bb sha256sum -c SHA256SUMS
if [ "$mode" = --verify ]; then
    echo 'Bundle checksums verified.'
    exit 0
fi

[ "$(id -u)" = 0 ] || { echo 'Root is required.' >&2; exit 1; }
device=$(getprop ro.product.device)
if [ "$device" != totoro ] && [ -r /system/build.prop ]; then
    device=$(bb sed -n 's/^ro.product.device=//p' /system/build.prop | bb head -n 1)
fi
[ "$device" = totoro ] || { echo 'This bundle requires GT-S5360/totoro.' >&2; exit 1; }
bb awk '$1 == "mtd6:" && $2 == "00500000" && $4 == "\"boot\"" { found=1 } END { exit !found }' /proc/mtd
[ -c /dev/mtd/mtd6ro ]
[ -d /system/lib/modules ]
[ -x /system/bin/flash_image ]

hash_file() {
    result=$(bb sha256sum "$1") || return 1
    printf '%s' "${result%% *}"
}
verify_pair() {
    directory=$1
    for module in "$directory"/modules/*.ko; do
        [ -f "$module" ] || return 1
        name=${module##*/}
        [ "$(hash_file "$module")" = "$(hash_file "/system/lib/modules/$name")" ] || return 1
    done
    bytes=$(bb wc -c < "$directory/boot.img") || return 1
    [ $((bytes % 4096)) = 0 ] || return 1
    [ "$bytes" -le 5242880 ] || return 1
    result=$(bb dd if=/dev/mtd/mtd6ro bs=4096 count=$((bytes / 4096)) 2>/dev/null | bb sha256sum) || return 1
    [ "${result%% *}" = "$(hash_file "$directory/boot.img")" ]
}
write_pair() {
    directory=$1
    for module in "$directory"/modules/*.ko; do
        [ -f "$module" ] || return 1
        name=${module##*/}
        bb cp "$module" "/system/lib/modules/.$name.port239" || return 1
        bb chown 0:0 "/system/lib/modules/.$name.port239" || return 1
        bb chmod 0644 "/system/lib/modules/.$name.port239" || return 1
        bb mv "/system/lib/modules/.$name.port239" "/system/lib/modules/$name" || return 1
    done
    sync || return 1
    /system/bin/flash_image boot "$directory/boot.img" || return 1
    sync || return 1
    verify_pair "$directory"
}

if [ "$mode" = --install ]; then
    [ "$(getprop ro.build.version.release)" = 4.0.4 ] || { echo 'The candidate ramdisk targets CM9 Android 4.0.4.' >&2; exit 1; }
    verify_pair rollback || { echo 'Installed boot/modules differ from the bundled rollback baseline.' >&2; exit 1; }
    selected=payload
else
    selected=rollback
fi
was_readonly=0
if bb awk '$2 == "/system" && $4 ~ /(^|,)ro(,|$)/ { found=1 } END { exit !found }' /proc/mounts; then
    was_readonly=1
fi
dirty=0
cleanup() {
    status=$?
    trap - 0
    if [ "$status" != 0 ] && [ "$dirty" = 1 ]; then
        echo 'Write or readback failed; restoring the bundled working boot/module pair.' >&2
        if write_pair rollback; then
            echo 'Rollback readback verified.' >&2
        else
            echo 'Rollback verification failed. Use the recovery procedure in INSTALL.md before rebooting.' >&2
        fi
    fi
    if [ "$was_readonly" = 1 ]; then
        bb mount -o remount,ro /system || true
    fi
    exit "$status"
}
trap cleanup 0
bb mount -o remount,rw /system
dirty=1
write_pair "$selected"
dirty=0
echo 'Boot and all modules verified. Reboot separately when ready.'
