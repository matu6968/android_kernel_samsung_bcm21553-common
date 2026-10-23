# Galaxy Y AOA backport: recorded results

Test date: 23 October 2026. Hardware: GT-S5360/totoro, BCM21553,
CM9 Android 4.0.4/API 15. Only this phone was modified. The network-connected
SM-G361F was left untouched. The installed Android framework and system modules
were retained.

## Implementation and builds

The working branch is `aoa/ics`, based on ICS commit
`41a2676a650e5e7d92c4c1530729a1aad5292bb0`. Neither original branch had an
accessory driver. Their USB sources matched; branch differences concerned
Bluetooth configuration. The patch also builds on cm-11.0 commit
`ac3ff8d6bee57e51e49ed0502efee3c467e3ebc5` with that branch's Bluetooth settings.

| Configuration | Result | zImage bytes |
| --- | --- | ---: |
| ICS Totoro, AOA enabled | PASS | 2,835,492 |
| cm-11.0 Totoro, AOA enabled | PASS | 2,765,844 |
| ICS Totoro, AOA disabled | PASS | 2,830,212 |

Compiler: repository-documented ARM EABI GCC 4.6, pinned toolchain commit
`d73a051b1fd1d98f5c2463354fb67898f0090bdb`; host compiler GCC 11.
The ICS configuration differs from `/proc/config.gz` by the generation date
and `CONFIG_USB_ANDROID_ACCESSORY=y`. Existing modpost section-mismatch warnings
remain. Final USB source files were compared against the built cm-11.0 worktree.

## Installed boot image and backups

The installed image is `boot.aoa.img`, 4,616,192 bytes, leaving **626,688 bytes**
within the 5,242,880-byte partition. SHA-256:
`17623cc2681b52249aefaeac614ca6f9dbd89831765701116dfb8a00e0e86e32`.
It was installed with the phone's NAND-aware `flash_image` utility; a raw
readback over the written length matched that hash. Running kernel: 2.6.35.7,
build #9, 23 October 2026 12:24:58 CEST.

Header addresses, command line, page size and second-image contents were
preserved. All 252 cpio entries retained their metadata and recovery contents.
Only `init.rc` and `init.gt-s5360board.usb.rc` changed. USB property handlers now
use the Broadcom ICS frontend while retaining adbd and tethering actions.

Original boot, boot-backup and all eight installed modules were backed up.
All eight module checksums still match. `boot_backup` was never written; its
readback SHA-256 remains
`16f34632ed5fc8556743bb4a5b403edd2cf9845372951b48914b61597a4a0ba7`.

`boot.rollback.img` is the exact packed prefix of the original boot backup,
4,694,016 bytes, SHA-256
`d4cf62cf4e002c99b3655e0a2d2ffd7c72737a28ba836f503fa7b2b818e79022`.
The accompanying tar contains the same image as `boot.img`. The restore script
checks root, model, input checksum and NAND readback. Its syntax, image contents
and 1,146-page readback length were checked. **The rollback image was flashed,
read back and boot-tested:** the original GCC 4.4.3 kernel, build #1 dated
10 September 2018, ran and `/dev/usb_accessory` was absent. Once Android started,
USB ADB went offline with the reverse `adb,mass_storage` ordering; network ADB
was enabled to reinstall the AOA image. The delivered restore script selects
`mass_storage,adb`, supported by the original init handlers. Reboot is a separate
ADB command or recovery-menu action, since BusyBox lacks a reboot applet.
Recovery contents were preserved, not entered/tested.

## Hardware results on CM9

| Test | Measured result |
| --- | --- |
| Boot, USB ADB, Wi-Fi | PASS; original Wi-Fi module loaded |
| Accessory device permissions | `/dev/usb_accessory`, root:usb, mode 0660 |
| Request 51 | Protocol version 1, bytes `01 00` |
| Identification ABI | All six strings and start-request flag read through native ioctls |
| Request validation | Ten malformed setup cases stalled; valid protocol request succeeded after each |
| Maximum string | 256-byte control write accepted and safely terminated |
| Accessory only | `18d1:2d00`, one interface, accessory number 0 |
| Accessory plus ADB | `18d1:2d01`, accessory 0, ADB 1; USB ADB commands succeeded |
| Android service | `dumpsys usb` reported the accessory and all six correct metadata values |
| API-15 application I/O | Real `UsbManager.openAccessory()` echo passed in both modes on final build #9 |
| Transfer boundaries | 1, 63, 64, 65, 511, 512, 513, 4095, 4096, 4097, 16383, 16384, 16385, 65537, 1048576 bytes |
| Transfer integrity | Every byte matched; each complete suite transferred 1,177,282 bytes each direction |
| Reopen and repeated negotiation | Full transfer suites passed after app reopen and after renegotiation |
| Blocked read + physical unplug | Returned `-ENODEV`; reconnect restored default configuration |
| Blocked write + software disconnect | Returned `-EIO` promptly after exhausting available TX requests |
| Bluetooth | Enabled successfully; original Bluetooth state retained/restored |
| Charging | USB power and charging status reported; removal/insertion events observed |
| Mass storage | Read-only USB BOT inquiry and capacity: Samsung S5360 Card, 8 MiB scratch LUN, 512-byte blocks |
| RNDIS | `04e8:6863`, two interfaces, host `rndis_host` bound, host link UP/LOWER_UP and device usb0 UP |
| Full-speed descriptors | Other-speed bulk endpoint descriptors advertised 64-byte maximum packets |

The scratch mass-storage file was detached and cleared. No actual SD-card data
was exported for the BOT test. Both mass-storage/ADB function orderings were
accepted. No USB panic/Oops was observed in the captured logs; existing vendor
GPIO, regulator and Wi-Fi warnings are present.

## Observed limitations and validation scope

One negotiation immediately following a TCP-to-USB adbd transition stalled
descriptor enumeration, fell back to the default mode and required a cable
reconnect. The phone stayed responsive over Wi-Fi. Subsequent USB ADB,
renegotiation and full bulk-transfer retries passed. **This intermittent
enumeration failure remains unresolved.** Logs are included under `results`.
The original 2018 kernel also produced descriptor-read errors (`-71`) during
the rollback boot, followed by successful enumeration. This establishes that
similar descriptor errors occur without the backport; it does not establish
the cause of the stalled accessory transition.

The test app must be stopped before a new negotiation if it retains an old
accessory file: the preserved ICS ABI has one exclusive open, also used by the
framework's metadata reader. An Activity needs time to start its read thread;
the host tool waits one second before sending data. Debugging-toggle transitions
can leave CM9's accessory metadata cached until disconnect/reboot.

RNDIS DHCP/Internet forwarding and physical full-speed-host operation were not
tested. cm-11.0 received build validation only. AOA audio and HID are excluded.
Final state is normal `adb,mass_storage`, with Android debugging enabled and
network ADB disabled. The echo APK remains installed as a test tool.

See `README.md` for build, repacking and test commands. The delivery includes
source patch, configs/build logs, zImage files, boot and rollback artifacts,
test APK/native probe, source test tools, backups, raw results and checksums.
