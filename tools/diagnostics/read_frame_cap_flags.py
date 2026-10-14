"""Read t.MaxFPS ownership for the pinned game build through read-only /proc memory.

The addresses and getter instructions are verified in frame-cap-20261015.
This helper does not call native code or write process memory.
"""
import hashlib
import os
from pathlib import Path
import struct


EXE_SHA256 = '363322022a0fdfd0cf87c494619da91e0985e6435d105a37def8a89cb3e29922'


class FrameCapFlags:
    def __init__(self, pid):
        self.pid = pid
        self.proc = Path('/proc') / str(pid)
        self.started = self.start_time()
        rows = [line.split() for line in (self.proc / 'maps').read_text().splitlines()]
        images = [row for row in rows if len(row) > 5 and
                  row[-1].endswith('MortalShell2-Win64-Shipping.exe') and int(row[2], 16) == 0]
        if len(images) != 1:
            raise RuntimeError('Expected one mapped game image')
        self.base = int(images[0][0].split('-')[0], 16)
        self.exe = Path(images[0][-1])
        with self.exe.open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != EXE_SHA256:
                raise RuntimeError('Frame-cap reader does not support this executable')
        first = self.read()
        if first['flags'] & 0xff000000 == 0:
            raise RuntimeError('Constructor priority would be promoted by the native setter')
        self.initial = first

    def start_time(self):
        return (self.proc / 'stat').read_text().rsplit(')', 1)[1].split()[19]

    def read(self):
        if self.start_time() != self.started:
            raise RuntimeError('Game process was replaced')
        regions = [line.split() for line in (self.proc / 'maps').read_text().splitlines()]
        fd = os.open(self.proc / 'mem', os.O_RDONLY)
        try:
            def data(address, count):
                if not any(int(r[0].split('-')[0], 16) <= address and
                           address + count <= int(r[0].split('-')[1], 16) and r[1].startswith('r')
                           for r in regions):
                    raise RuntimeError('Frame-cap address is not readable')
                value = os.pread(fd, count, address)
                if len(value) != count:
                    raise RuntimeError('Incomplete frame-cap read')
                return value

            def ptr(address):
                return struct.unpack('<Q', data(address, 8))[0]

            engine = ptr(self.base + 0xb3c7a78)
            if ptr(ptr(engine) + 0x318) != self.base + 0x45cd180:
                raise RuntimeError('Engine frame-cap setter changed')
            cvar = ptr(self.base + 0xb3c7c10)
            getter = ptr(ptr(cvar) + 0x18)
            if getter != self.base + 0x127c470 or data(getter, 4) != bytes.fromhex('8b4118c3'):
                raise RuntimeError('Console flag getter changed')
            flags = struct.unpack('<I', data(cvar + 0x18, 4))[0]
            return dict(pid=self.pid, module_base=hex(self.base), engine=hex(engine),
                        cvar=hex(cvar), getter=hex(getter), flags=flags, set_by=flags & 0xff000000)
        finally:
            os.close(fd)
