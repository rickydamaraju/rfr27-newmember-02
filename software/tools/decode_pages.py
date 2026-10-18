#!/usr/bin/env python3
"""Decode three 24-byte hex packets; I2C adapter and stale timer are host-specific."""
import argparse
import json
import struct

def crc8(data):
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ (0x07 if crc & 0x80 else 0)) & 0xff
    return crc

def decode(pages):
    if len(pages) != 3:
        raise ValueError('Expected three pages')
    for i, page in enumerate(pages):
        if len(page) != 24 or page[0] != 1 or page[1] != i:
            raise ValueError('Wrong length, protocol version or page ID')
        if crc8(page[:23]) != page[23]:
            raise ValueError('CRC mismatch')
    sequence = [struct.unpack_from('<H', p, 2)[0] for p in pages]
    if len(set(sequence)) != 1:
        raise ValueError('Mixed scans: retry all pages within a bounded timeout')
    if any(p[4] != pages[0][4] for p in pages):
        raise ValueError('Inconsistent flags')
    if pages[1][5] != 0 or pages[2][5] != 8:
        raise ValueError('Wrong channel range')
    p = pages[0]
    valid, calibrated = struct.unpack_from('<HH', p, 6)
    def temp(v): return None if v == -32768 else v / 100
    temperatures = [temp(v) for page in pages[1:] for v in struct.unpack_from('<8h', page, 6)]
    maximum, minimum, mean = map(temp, struct.unpack_from('<3h', p, 10))
    ready = not bool(p[4] & 8)
    return dict(sequence=sequence[0], flags=p[4], valid_mask=valid,
                calibrated_mask=calibrated, max_channel=None if p[5] == 255 else p[5],
                maximum_c=maximum, minimum_c=minimum, mean_c=mean,
                temperature_c=temperatures, uptime_ms=struct.unpack_from('<I', p, 16)[0],
                all_readings_validated=ready and valid == 0xffff and calibrated == 0xffff)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pages', nargs=3, help='24-byte page as a quoted hexadecimal string')
    args = parser.parse_args()
    print(json.dumps(decode([bytes.fromhex(p) for p in args.pages]), indent=2))
