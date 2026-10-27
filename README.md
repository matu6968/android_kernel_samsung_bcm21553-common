# BCM21553 Linux 3.2.102 kernel

Samsung/Broadcom support for Galaxy Y (`totoro`), Galaxy Ace VE (`cooperve`)
and Galaxy Mini VE (`tassve`), retaining the Android and AOA changes, on the
last release of the Linux 3.2 series.

See [the port documentation](Documentation/bcm21553-port32.md) for branch
selection, the pinned compiler, the changes needed for 3.2, the OneNAND layout
caveat and what has been tested.

```sh
export CROSS_COMPILE=/path/to/arm-eabi-4.6/bin/arm-eabi-
HOSTCC=gcc-11 tools/port239/build.sh /path/to/output totoro default
```

Both kernels and modules must be installed together. **Read the OneNAND section
of the port documentation before flashing**: a 3.0 kernel without the pre-3.0
128 byte OOB layout erases the YAFFS partitions. `totoro` has been booted on a
GT-S5360 with CM9 (Wi-Fi, Bluetooth and the modem daemon work; calls and data
have not been tested). Original vendor work originates from
[LineageOS-ARMv6](https://github.com/LineageOS-ARMv6/android_kernel_samsung_bcm21553-common).
