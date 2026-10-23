# BCM21553 Linux 2.6.39.4 kernel

Samsung/Broadcom support for Galaxy Y (`totoro`), Galaxy Ace VE (`cooperve`)
and Galaxy Mini VE (`tassve`), retaining the Android and AOA changes.

See [the port documentation](Documentation/bcm21553-port239.md) for branch
selection, the pinned compiler, compatibility details and validation scope.

```sh
export CROSS_COMPILE=/path/to/arm-eabi-4.6/bin/arm-eabi-
HOSTCC=gcc-11 tools/port239/build.sh /path/to/output totoro default
```

Both kernels and modules must be installed together. The delivery includes
CM9/totoro installation and rollback materials; no device was flashed or
runtime-tested for this port. Original vendor work originates from
[LineageOS-ARMv6](https://github.com/LineageOS-ARMv6/android_kernel_samsung_bcm21553-common).
