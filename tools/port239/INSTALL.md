> **Note for the 3.0.101 branches:** this guide was written for the 2.6.39.4
> delivery bundle, and `apply_bundle.sh` has not been used with 3.0.101 images;
> the 3.0.101 kernel was tested by flashing `boot` with Heimdall and installing the
> eight modules from recovery. The same paired-install rules apply: the boot image
> and all eight modules (built from the same tree) go together, and the rollback
> set must be available before flashing. See `Documentation/bcm21553-port30.md`.

# CM9 / GT-S5360 installation bundle

This candidate is built and checked on the host. It has not been booted on a
phone. It targets GT-S5360/totoro running CM9 Android 4.0.4 and the exact known
AOA boot/module baseline included under `rollback/`.

Keep this complete bundle and its external archive checksum on the host. It
contains `payload/boot.img`, eight matching 2.6.39.4 modules, and the known
working 2.6.35.7 AOA boot image plus all eight original modules for rollback.
The old image was hardware-tested during the earlier AOA work; that result
does not validate the new kernel or this installer.

## Verify and install

1. Verify the downloaded archive against its adjacent SHA-256 file, extract
   it, and check `SHA256SUMS`. Copy the entire extracted directory to the
   phone's internal `/data/local/tmp/port239`, preserving both sets of files.
   Keep an independent host copy. Select this phone explicitly in ADB commands.
2. Arrange access to the existing recovery and a host copy of the rollback
   files before trying the candidate. Use a charged battery and leave the
   phone connected while writing and checking the image.
3. In a root shell on the phone, verify the bundle:

   ```sh
   cd /data/local/tmp/port239
   sh apply_bundle.sh --verify
   ```

4. Install the paired image and modules:

   ```sh
   sh apply_bundle.sh --install
   ```

   The script checks the model, Android release, boot partition and current
   boot/module hashes. It remounts `/system`, replaces the eight modules, uses
   `/system/bin/flash_image boot` to write NAND, and verifies module hashes and
   raw boot readback. It restores the prior `/system` mount mode. A failed
   write/readback attempts to restore the complete rollback set.
5. After verification succeeds, reboot separately through ADB or the recovery
   menu. Capture boot/kernel logs and follow the hardware checklist in the
   port documentation. The script does not change `boot_backup` or reboot.

The installer deliberately requires the known rollback baseline before an
installation. If that check fails, make a fresh complete backup of the actual
working boot and modules and regenerate the bundle for that baseline.

## Rollback

If Android still starts, open a root shell and run:

```sh
cd /data/local/tmp/port239
sh apply_bundle.sh --restore
```

If Android cannot start, enter the existing recovery, mount `/system` and
`/data`, and obtain a root recovery shell. Use the saved bundle from internal
storage or copy the host copy into the recovery's `/tmp/port239`. Run the same
`--verify` and `--restore` commands there. The script can identify totoro from
the mounted `/system/build.prop` when recovery properties differ. It requires
BusyBox and the installed NAND-aware `/system/bin/flash_image` utility.

Successful restoration verifies both the original modules and the boot
readback. Reboot separately. If a recovery environment cannot run the script,
the equivalent sequence is: verify the bundle checksums, copy every
`rollback/modules/*.ko` to `/system/lib/modules` with root ownership and mode
0644, sync, run `/system/bin/flash_image boot rollback/boot.img`, sync, and
compare readback and installed-module hashes against the rollback files.
Never substitute a raw `dd` write for the NAND-aware writer.

Recovery entry and installation/rollback of this new port remain hardware
validation steps. Preserve the existing `boot_backup` partition throughout.
CM11 kernels must be paired with a CM11 ramdisk; this CM9 image is not a CM11
installation package.
