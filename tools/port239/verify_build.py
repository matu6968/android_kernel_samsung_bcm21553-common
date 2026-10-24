#!/usr/bin/env python3
"""Check the Galaxy Y port's static build invariants; never access a device."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import struct
import subprocess

EXPECTED_MODULES = {
    'bcmdhd.ko', 'brcm-headsetsw.ko', 'brcm_switch.ko', 'brcm_test_drv.ko',
    'cifs.ko', 'h6270enc.ko', 'hx170dec.ko', 'oprofile.ko',
}

def require(condition, message):
    if not condition:
        raise ValueError(message)

def config(path):
    values = {}
    for line in path.read_text().splitlines():
        if line.startswith('CONFIG_'):
            key, value = line.split('=', 1)
            values[key] = value
        elif line.startswith('# CONFIG_') and line.endswith(' is not set'):
            values[line[2:-11]] = 'n'
    return values

def modinfo(path):
    raw = path.read_bytes()
    require(raw[:6] == b'\x7fELF\x01\x01', f'{path}: expected ELF32 little endian')
    require(struct.unpack_from('<H', raw, 18)[0] == 40, f'{path}: expected ARM')
    section_offset = struct.unpack_from('<I', raw, 32)[0]
    entry_size, count, names_index = struct.unpack_from('<3H', raw, 46)
    sections = [struct.unpack_from('<10I', raw, section_offset + i * entry_size)
                for i in range(count)]
    names_header = sections[names_index]
    names = raw[names_header[4]:names_header[4] + names_header[5]]
    for section in sections:
        name = names[section[0]:].split(b'\0', 1)[0]
        if name == b'.modinfo':
            fields = raw[section[4]:section[4] + section[5]].split(b'\0')
            return dict(item.decode().split('=', 1) for item in fields if b'=' in item)
    raise ValueError(f'{path}: no module metadata')

def verify(source, build, device, aoa):
    cfg = config(build / '.config')
    for name in ['CPU_V6K', 'CPU_32v6K', 'HW_PERF_EVENTS', 'AEABI', 'MTD_NAND_BRCM_NVSRAM',
                 'YAFFS_FS', 'YAFFS_YAFFS2', 'MMC_BCM', 'BCM215XX_PM',
                 'ASHMEM', 'ANDROID_PMEM', 'ANDROID_BINDER_IPC', 'ANDROID_LOGGER',
                 'ANDROID_LOW_MEMORY_KILLER', 'WAKELOCK', 'EARLYSUSPEND',
                 'NETFILTER_XT_MATCH_QTAGUID', 'USB_ANDROID', 'USB_GADGET_DWC_OTG',
                 'NET_IPGRE', 'NETFILTER_XT_MATCH_ADDRTYPE', 'CRYPTO_MD4']:
        require(cfg.get('CONFIG_' + name) == 'y', f'Missing required feature: {name}')
    require(cfg.get('CONFIG_CRYPTO_MANAGER_DISABLE_TESTS') == 'n', 'Crypto self-tests disabled')
    require(cfg.get('CONFIG_BOARD_' + device.upper()) == 'y', 'Wrong board')
    require(cfg.get('CONFIG_SDRAM_BASE_ADDR') == '0x81600000', 'SDRAM base changed')
    require(cfg.get('CONFIG_AUTO_ZRELADDR') == 'n', 'Use the explicit vendor load address')
    expected_aoa = aoa == 'on' or (aoa == 'default' and device == 'totoro')
    require((cfg.get('CONFIG_USB_ANDROID_ACCESSORY') == 'y') == expected_aoa,
            'Unexpected AOA selection')
    release = (build / 'include/config/kernel.release').read_text().strip()
    makefile = (source / 'Makefile').read_text()
    expected_release = '.'.join(re.search(rf'^{key} = (\d+)', makefile, re.M).group(1)
                                for key in ('VERSION', 'PATCHLEVEL', 'SUBLEVEL'))
    require(release == expected_release, f'Unexpected kernel release: {release} (source is {expected_release})')
    zimage = build / 'arch/arm/boot/zImage'
    require(struct.unpack_from('<I', zimage.read_bytes(), 0x24)[0] == 0x016f2818,
            'Invalid ARM zImage header')
    modules = []
    for item in (build / 'modules.order').read_text().splitlines():
        relative = item.removeprefix('kernel/')
        path = build / relative
        fields = modinfo(path)
        require(fields.get('vermagic', '').startswith(release + ' '), f'{item}: wrong vermagic')
        modules.append({'path': relative, 'name': path.name,
                        'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                        'vermagic': fields['vermagic'], 'depends': fields.get('depends', '')})
    require(len({m['name'] for m in modules}) == len(modules), 'Duplicate module filename')
    if device == 'totoro':
        require({m['name'] for m in modules} == EXPECTED_MODULES, 'Totoro module inventory changed')
    require(len({m['vermagic'] for m in modules}) == 1, 'Modules disagree on vermagic')
    def git(*args):
        return subprocess.check_output(['git', '-C', str(source), *args], text=True).strip()
    result = {'validation': 'build/static only; hardware not tested', 'device': device,
              'aoa': expected_aoa, 'release': release,
              'source_commit': git('rev-parse', 'HEAD'),
              'source_branch': git('branch', '--show-current'),
              'source_dirty': bool(git('status', '--porcelain')),
              'compiler': subprocess.check_output([os.environ.get('CROSS_COMPILE', '') + 'gcc', '--version'], text=True).splitlines()[0],
              'host_compiler': subprocess.check_output([os.environ.get('HOSTCC', 'gcc-11'), '--version'], text=True).splitlines()[0],
              'zimage_bytes': zimage.stat().st_size,
              'zimage_sha256': hashlib.sha256(zimage.read_bytes()).hexdigest(),
              'config_sha256': hashlib.sha256((build / '.config').read_bytes()).hexdigest(),
              'modules': modules}
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--device', choices=['totoro', 'cooperve', 'tassve'], required=True)
    parser.add_argument('--aoa', choices=['default', 'on', 'off'], default='default')
    args = parser.parse_args()
    report = verify(args.source.resolve(), args.build.resolve(), args.device, args.aoa)
    (args.build / 'build-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f"PASS: {args.device}, {report['release']}, {len(report['modules'])} modules, "
          f"zImage {report['zimage_bytes']} bytes; hardware untested")

if __name__ == '__main__':
    main()
