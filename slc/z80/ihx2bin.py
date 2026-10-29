#!/usr/bin/env python3
import os
import argh
import struct


def ihx2bin(src: str, dst: str):
    line_number = 0
    if not os.path.exists(dst):
        with open(dst, 'wb') as f:
            pass
    with open(dst, 'r+b') as f:
        for line in open(src).readlines():
            line_number += 1
            if line[0] != ':':
                continue
            line = line.strip()
            data = bytes.fromhex(line[1:-2])
            calculated_checksum = (0x100 - (sum(data) & 0xFF)) & 0xFF
            given_checksum = bytes.fromhex(line[-2:])[0]
            if calculated_checksum == given_checksum:
                length = data[0]
                address = struct.unpack('>H', data[1:3])[0]
                data_type = data[3]
                if data_type == 0 and length > 0:
                    data = data[4:]
                    f.seek(address)
                    f.write(data)
            else:
                print(f"Checksum Error at line {line_number}\n{line}")
                return


if __name__ == '__main__':
    argh.dispatch_command(ihx2bin)
