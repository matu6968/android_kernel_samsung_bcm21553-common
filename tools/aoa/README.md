> This is the historical 2.6.35.7 AOA backport record. On the 3.2.102 and 3.0.101 port
> branches, use [the port documentation](../../Documentation/bcm21553-port32.md).
> The hardware results below were obtained on 2.6.35.7 and do not validate the
> newer kernels.

# Galaxy Y AOA 1.0 backport

This targets the GT-S5360 (`totoro`), BCM21553, Linux 2.6.35.7 and the
installed CyanogenMod 9 / Android 4.0.4 framework. The USB patch also applies
to `cm-11.0`; only CM9 is tested on hardware. It adds accessory data and
optional ADB. AOA 2.0 HID and audio are outside this implementation.

## Source and interfaces

The active driver is `drivers/usb/gadget_brcm`, selected by `drivers/Makefile`
and `drivers/usb/Kconfig`. The generic `drivers/usb/gadget` is not built for
this device. `CONFIG_USB_ANDROID_ACCESSORY` defaults off and is enabled in
`cyanogenmod_totoro_defconfig`.

The accessory driver and ioctl ABI originate from AOSP kernel/samsung commit
`665612374970f5dba43b65510b52b72130818541`, branch
`android-samsung-3.0-ics-mr1`. The backport fixes its duplicate OUT-endpoint
allocation and disconnect waits, and adds serialized I/O and teardown handling.
The Broadcom ADB driver now claims its endpoint pair before accessory binding.

The phone exposes `/dev/usb_accessory`, the six ICS string ioctls and
`ACCESSORY_IS_START_REQUESTED`. AOA vendor requests 51, 52 and 53 are handled
before configuration dispatch. IDs are `18d1:2d00` and `18d1:2d01`; accessory
is interface 0, and ADB is interface 1 when enabled. Descriptor numbering and
interface dispatch use the same selected-function order.

The ICS frontend provides `/sys/class/android_usb/android0/{enable,functions,state}`,
descriptor attributes, the existing LUN at `f_mass_storage/lun/file`, and
`f_rndis/ethaddr`. `functions` accepts `adb`, `mass_storage`, their combination
in either order, `rndis`, `rndis,adb`, `accessory`, and `accessory,adb`.
RNDIS retains the vendor limitation: `rndis,adb` exposes RNDIS without USB ADB.
Compositions and descriptors are staged with `enable=0`, then committed with
`enable=1`. Unsupported, duplicate and conflicting functions are rejected.
Legacy adbd enable-node opens cannot override an ICS-controlled composition.

The Broadcom EP0 implementation requires explicit IN status for START. With
AOA enabled it also records control-OUT DMA byte counts, receives control writes
one packet at a time, and stalls malformed AOA OUT data requests correctly.
These fixes allow all 256-byte accessory identification strings.

## Build

Use Android ARM EABI GCC 4.6, pinned to toolchain commit
`d73a051b1fd1d98f5c2463354fb67898f0090bdb`, from the
`android-4.4.4_r2` tag. A full resolved hash is included with the
build artifacts. The toolchain repository is:
`https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6`.
Modern HEAD contains no compiler; check out the historical tag.

```sh
export CROSS_COMPILE=/path/to/arm-eabi-4.6/bin/arm-eabi-
HOSTCC=gcc-11 tools/aoa/build_kernel.sh /path/to/build on
```

Out-of-tree builds preserve the source checkout. The `off` argument verifies
the disabled configuration. The boot image keeps release `2.6.35.7`, matching
the installed module vermagic; avoid unrelated config changes.

## Boot image and rollback

First save both **boot** and **boot_backup** and the installed modules from
this exact phone. Keep the backups on the host. `repack_boot.py` accepts only
this device's v0 Android header with 4096-byte pages and no DT/second extension.
It preserves addresses, command line, second image, and all cpio metadata,
symlinks, hardlink identities and recovery files. Only `init.rc` and
`init.gt-s5360board.usb.rc` change. Existing generic accessory handlers are
retained; conflicting legacy handlers are replaced. The output is rejected if
it exceeds the 5 MiB boot partition.

```sh
python3 tools/aoa/repack_boot.py --original boot.original.img \
  --kernel /path/to/build/arch/arm/boot/zImage --output boot.aoa.img
```

Install with the existing NAND-aware `/system/bin/flash_image boot IMAGE` on
the verified Totoro, then sync and compare a SHA-256 readback from
`/dev/mtd/mtd6ro` over the written image length before rebooting. Do not write
`boot_backup`. Preserve the original boot image for rollback using the same
utility from Android or an appropriate mounted recovery environment. A
checksum-verified rollback image, tar and restore script accompany this run.
The rollback script selects `mass_storage,adb`, the ordering understood by the
original board init. It leaves rebooting to the caller:

```sh
adb -s GALAXY_Y_SERIAL shell sh /data/local/tmp/aoa/restore-boot.sh
adb -s GALAXY_Y_SERIAL reboot
```

Keep network ADB available when testing the original kernel. Its USB ADB can
remain offline once CM9 starts with the reverse function ordering. The recorded
rollback boot used network ADB to reinstall the new image. This phone's BusyBox
does not include a reboot applet; use the ADB reboot command or recovery menu.

## Tests

The host tool requires PyUSB and libusb. Its physical USB port restriction
(default bus 1, port 2) avoids operating on another attached phone. Supply
`--bus` and `--ports` explicitly when using a different port. Do not run USB
control tests concurrently. `probe` and `malformed` reset negotiation strings;
run them in normal USB mode before `start`.

```sh
python3 tools/aoa/host.py probe
python3 tools/aoa/host.py malformed
python3 tools/aoa/host.py start
python3 tools/aoa/host.py echo
```

Build the echo APK using Android SDK platform 15, build-tools 36 and a JDK:

```sh
ANDROID_SDK_ROOT=/path/to/sdk tools/aoa/build_echo.sh /path/to/echo-build
adb -s GALAXY_Y_SERIAL install -r /path/to/echo-build/aoa-echo.apk
```

The app uses `UsbManager.getAccessoryList()`, requests permission and opens the
accessory through `UsbManager.openAccessory()`. It echoes the byte stream and
logs totals under `AOAEcho`. The host tests packet boundaries through 1 MiB.
The host allows one second for the Activity's reader thread to start. Stop the
test app before starting a new negotiation: the ICS misc-device ABI permits
only one open file, including the framework's metadata reader.
Short transfers whose last OUT packet is full are terminated with a zero-length
packet, since the app reads 16 KiB at a time.

`Control` is an optional root-only `app_process` helper in the test APK. It can
request a USB function, check Bluetooth or grant the test app access to
the current accessory through Android's USB service. It never runs inside the
installed framework. To return to the default composition:

```sh
adb -s GALAXY_Y_SERIAL shell 'apk=$(pm path org.galaxyy.aoa); CLASSPATH=${apk#package:} app_process /system/bin org.galaxyy.aoa.Control function default'
```

`probe.c` is a freestanding ARM EABI ioctl/blocking-read probe. Build with the
same cross compiler using `-Os -march=armv6 -marm -ffreestanding -fno-builtin
-nostdlib -static -Wl,-e,_start`. It prints all six kernel strings and the start
flag; its `read` argument blocks until data or disconnect, and `write` tests
blocked output without a host reader. This independently
checks the ABI and disconnect behavior without a host libc dependency.

Keep an alternate connection to the **Galaxy Y** for accessory-only and
RNDIS tests. Old ICS adbd switches between TCP and USB transports. Verify
`dumpsys usb`, native metadata, actual bulk data, ADB in combined mode,
disconnect cleanup, default-mode restoration, mass-storage and RNDIS
regressions. Kernel/framework log baselines contain pre-existing vendor GPIO,
regulator and Wi-Fi warnings; distinguish them from new USB failures.

The recorded run passed both accessory modes and repeated negotiation, but one
transition after switching adbd from TCP to USB stalled descriptor enumeration.
A cable reconnect restored it; the subsequent USB ADB and bulk-transfer retry
passed. This remains an observed reliability limitation, not a resolved defect.
The original 2018 kernel also produced host descriptor-read errors during the
rollback boot; those errors are not unique to the backport.
The delivery's `VALIDATION.md` distinguishes measured results from untested
items. RNDIS was checked through enumeration and link state, without a DHCP or
Internet-through-tethering test. Full-speed descriptors were checked from the
high-speed link; a physical full-speed host was not tested.
