#!/usr/bin/env python3
"""AOA 1.0 tests, restricted to one USB physical port (default bus 1, port 2)."""
import argparse
import errno
import json
import random
import time
import usb.core
import usb.util

STRINGS = ['GalaxyY', 'AOA Echo Test', 'BCM21553 accessory backport test',
           '1.0', 'https://source.android.com/docs/core/interaction/accessories/aoa',
           'galaxyy-aoa-test']

def device(args, accessory=False, timeout=10):
    deadline = time.monotonic() + timeout
    while True:
        for d in usb.core.find(find_all=True):
            if d.bus != args.bus or tuple(d.port_numbers or ()) != args.ports:
                continue
            if accessory and (d.idVendor != 0x18d1 or d.idProduct not in (0x2d00, 0x2d01)):
                continue
            if d.idVendor not in (0x04e8, 0x18d1):
                raise RuntimeError('Unexpected device at selected port')
            if accessory:
                try:
                    d.get_active_configuration()
                except usb.core.USBError:
                    usb.util.dispose_resources(d)
                    continue
            return d
        if time.monotonic() >= deadline:
            raise RuntimeError('Target device not found at selected physical USB port')
        time.sleep(0.1)

def protocol(d):
    if d.idVendor == 0x18d1 and d.idProduct in (0x2d00, 0x2d01):
        raise RuntimeError('Return to the default USB mode before resetting negotiation strings')
    result = bytes(d.ctrl_transfer(0xc0, 51, 0, 0, 2, timeout=2000))
    if result != b'\x01\x00':
        raise RuntimeError(f'Expected AOA 1.0, got {result.hex()}')
    return 1

def start(args):
    d = device(args)
    protocol(d)
    for index, value in enumerate(STRINGS):
        d.ctrl_transfer(0x40, 52, 0, index, value.encode()+b'\0', timeout=2000)
    d.ctrl_transfer(0x40, 53, 0, 0, b'', timeout=2000)
    usb.util.dispose_resources(d)
    d = device(args, accessory=True, timeout=20)
    usb.util.dispose_resources(d)
    describe(args)

def describe(args):
    d = device(args, accessory=True)
    result = dict(vid=f'{d.idVendor:04x}', pid=f'{d.idProduct:04x}', bus=d.bus,
                  address=d.address, strings=STRINGS)
    cfg = d.get_active_configuration()
    result['interfaces'] = [dict(number=i.bInterfaceNumber, cls=i.bInterfaceClass,
                                subclass=i.bInterfaceSubClass, protocol=i.bInterfaceProtocol,
                                endpoints=[e.bEndpointAddress for e in i]) for i in cfg]
    other = bytes(d.ctrl_transfer(0x80, 6, 0x0700, 0, 512, timeout=2000))
    packets = []
    pos = 0
    while pos < len(other):
        length, kind = other[pos:pos+2]
        assert length >= 2 and pos+length <= len(other)
        if kind == 5 and other[pos+3] & 3 == 2:
            packets.append(int.from_bytes(other[pos+4:pos+6], 'little'))
        pos += length
    assert packets and all(n == 64 for n in packets), packets
    result['full_speed_bulk_max_packets'] = packets
    expected = 2 if d.idProduct == 0x2d01 else 1
    assert cfg.bNumInterfaces == expected, result
    assert result['interfaces'][0]['cls'] == 255
    assert result['interfaces'][0]['subclass'] == 255
    print(json.dumps(result, indent=2), flush=True)
    usb.util.dispose_resources(d)

def echo(args):
    # am start is asynchronous on ICS; allow its reader thread to start.
    time.sleep(1)
    d = device(args, accessory=True)
    cfg = d.get_active_configuration()
    interface = cfg[(0, 0)]
    ep_in = next(e for e in interface if usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN)
    ep_out = next(e for e in interface if usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT)
    usb.util.claim_interface(d, interface.bInterfaceNumber)
    rng = random.Random(21553)
    total = 0
    try:
        for size in (1, 63, 64, 65, 511, 512, 513, 4095, 4096, 4097,
                     16383, 16384, 16385, 65537, 1048576):
            payload = bytes(rng.randrange(256) for _ in range(size))
            received = bytearray()
            for offset in range(0, size, 16384):
                chunk = payload[offset:offset+16384]
                assert d.write(ep_out, chunk, timeout=10000) == len(chunk)
                # Terminate a short device read even when the last packet is full.
                if len(chunk) < 16384 and len(chunk) % ep_out.wMaxPacketSize == 0:
                    d.write(ep_out, b'', timeout=10000)
                echoed = bytearray()
                while len(echoed) < len(chunk):
                    echoed += bytes(d.read(ep_in, len(chunk)-len(echoed), timeout=10000))
                received += echoed
            assert bytes(received) == payload, f'Corruption at size {size}'
            total += size
            print(json.dumps(dict(test='echo', bytes=size, result='PASS')), flush=True)
        print(json.dumps(dict(test='echo_total', bytes=total, result='PASS')), flush=True)
    finally:
        usb.util.release_interface(d, interface.bInterfaceNumber)
        usb.util.dispose_resources(d)

def malformed(args):
    d = device(args)
    cases = [(0xc0, 51, 0, 0, 1), (0xc0, 51, 0, 0, 3),
             (0xc0, 51, 1, 0, 2), (0xc0, 51, 0, 1, 2),
             (0xc1, 51, 0, 0, 2), (0x40, 52, 0, 6, b'x\0'),
             (0x40, 52, 0, 0, b'x'*257), (0x40, 52, 0, 0, b''),
             (0x40, 53, 0, 1, b''), (0x40, 53, 0, 0, b'x')]
    for request in cases:
        try:
            d.ctrl_transfer(*request, timeout=2000)
        except usb.core.USBError as error:
            if error.errno != errno.EPIPE:
                raise
        else:
            raise AssertionError(f'Malformed request unexpectedly accepted: {request[:4]}')
        assert protocol(d) == 1
    # The maximum legal size must be accepted and safely terminated.
    d.ctrl_transfer(0x40, 52, 0, 0, b'x'*255+b'\0', timeout=2000)
    protocol(d)
    print(json.dumps(dict(test='malformed_setup', cases=len(cases), result='PASS')))
    usb.util.dispose_resources(d)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bus', type=int, default=1)
    parser.add_argument('--ports', type=lambda s: tuple(map(int, s.split('.'))), default=(2,))
    parser.add_argument('action', choices=['probe', 'start', 'describe', 'echo', 'malformed'])
    args = parser.parse_args()
    if args.action == 'start': start(args)
    elif args.action == 'describe': describe(args)
    elif args.action == 'echo': echo(args)
    elif args.action == 'malformed': malformed(args)
    else:
        d = device(args)
        print(json.dumps(dict(protocol=protocol(d), bus=d.bus, address=d.address)))
        usb.util.dispose_resources(d)

if __name__ == '__main__':
    main()
