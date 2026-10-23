#!/usr/bin/env python3
"""Read-only BOT check against the temporary 8 MiB test LUN on the selected phone."""
import argparse
import json
import struct
import usb.core
import usb.util
from host import device

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bus', type=int, default=1)
    p.add_argument('--ports', type=lambda s: tuple(map(int, s.split('.'))), default=(2,))
    args = p.parse_args()
    d = device(args)
    cfg = d.get_active_configuration()
    intf = next(i for i in cfg if i.bInterfaceClass == 8)
    ep_in = next(e for e in intf if e.bEndpointAddress & 0x80)
    ep_out = next(e for e in intf if not e.bEndpointAddress & 0x80)
    detached = d.is_kernel_driver_active(intf.bInterfaceNumber)
    if detached: d.detach_kernel_driver(intf.bInterfaceNumber)
    usb.util.claim_interface(d, intf.bInterfaceNumber)
    tag = 21553
    def scsi(cdb, length):
        nonlocal tag
        tag += 1
        cbw = struct.pack('<4sIIBBB16s', b'USBC', tag, length, 0x80, 0, len(cdb), bytes(cdb).ljust(16,b'\0'))
        assert d.write(ep_out, cbw, timeout=5000) == 31
        data = bytes(d.read(ep_in, 512, timeout=5000))
        if len(data) == 13 and data[:4] == b'USBS':
            csw, data = data, b''
        else:
            csw = bytes(d.read(ep_in, 512, timeout=5000))
        signature, returned_tag, residue, status = struct.unpack('<4sIIB', csw)
        assert signature == b'USBS' and returned_tag == tag
        return data, status
    try:
        inquiry, status = scsi([0x12,0,0,0,36,0],36)
        assert status == 0 and len(inquiry) == 36
        scsi([0x03,0,0,0,18,0],18)
        for _ in range(3):
            capacity, status = scsi([0x25,0,0,0,0,0,0,0,0,0],8)
            if not status: break
            scsi([0x03,0,0,0,18,0],18)
        assert status == 0 and len(capacity) == 8
        last, block = struct.unpack('>II',capacity)
        assert (last+1)*block == 8*1024*1024
        print(json.dumps(dict(test='mass_storage_bot', vendor=inquiry[8:16].decode().strip(),
                              product=inquiry[16:32].decode().strip(), bytes=(last+1)*block,
                              block_bytes=block, result='PASS')))
    finally:
        usb.util.release_interface(d, intf.bInterfaceNumber)
        if detached: d.attach_kernel_driver(intf.bInterfaceNumber)
        usb.util.dispose_resources(d)

if __name__ == '__main__': main()
