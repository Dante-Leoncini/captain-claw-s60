"""Minimal CLAW.REZ reader: rez.py CLAW.REZ list [prefix] | rez.py CLAW.REZ get <path> <out>"""
import struct
import sys


def read_dir(f, offset, size, prefix, out):
    end = offset + size
    pos = offset
    while pos < end:
        f.seek(pos)
        is_dir, off, sz, _date = struct.unpack('<IIII', f.read(16))
        if is_dir:
            name = b''
            while True:
                c = f.read(1)
                if c in (b'', b'\0'):
                    break
                name += c
            read_dir(f, off, sz, prefix + name.decode('latin1').lower() + '/', out)
            pos += 16 + len(name) + 1
        else:
            f.read(4)
            ext = f.read(4).rstrip(b'\0')[::-1].decode('latin1').lower()
            f.seek(pos + 28)
            name = b''
            while True:
                c = f.read(1)
                if c in (b'', b'\0'):
                    break
                name += c
            out[prefix + name.decode('latin1').lower() + '.' + ext] = (off, sz)
            pos += 28 + len(name) + 1 + 1


def index(path):
    f = open(path, 'rb')
    f.seek(127)
    _ver, off, sz = struct.unpack('<III', f.read(12))
    out = {}
    read_dir(f, off, sz, '/', out)
    return f, out


if __name__ == '__main__':
    f, files = index(sys.argv[1])
    if sys.argv[2] == 'list':
        pre = sys.argv[3].lower() if len(sys.argv) > 3 else ''
        for k in sorted(files):
            if k.startswith(pre):
                print(k, files[k][1])
    else:
        off, sz = files[sys.argv[3].lower()]
        f.seek(off)
        open(sys.argv[4], 'wb').write(f.read(sz))
