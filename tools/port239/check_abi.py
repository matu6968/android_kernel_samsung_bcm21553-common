#!/usr/bin/env python3
"""Cross-compile public Android ABI values against two source trees."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--cross-compile', required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    values = {}
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        for label, root in [('baseline', args.baseline), ('ported', args.source)]:
            root = root.resolve()
            obj = directory / (label + '.o')
            binary = directory / (label + '.bin')
            subprocess.run([
                args.cross_compile + 'gcc', '-c', '-march=armv6k', '-marm', '-ffreestanding',
                '-I' + str(here / 'abi-include'), '-I' + str(root / 'include'),
                '-I' + str(root / 'arch/arm/include'),
                '-I' + str(root / 'drivers/staging/android'),
                str(here / 'abi_probe.c'), '-o', str(obj)], check=True)
            subprocess.run([args.cross_compile + 'objcopy', '-O', 'binary',
                            '--only-section=.rodata', str(obj), str(binary)], check=True)
            raw = binary.read_bytes()
            values[label] = list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))
    same = values['baseline'] == values['ported']
    report = {'result': 'PASS' if same else 'FAIL', 'word_count': len(values['baseline']),
              'coverage': 'ARM32 pmem, ashmem, Binder, logger, AOA and V4L2 sizes/offsets/ioctls',
              'values': values}
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    if not same:
        raise SystemExit('Public ABI comparison failed; see ' + str(args.report))
    print(f"PASS: {len(values['baseline'])} ARM32 ABI values match")

if __name__ == '__main__':
    main()
