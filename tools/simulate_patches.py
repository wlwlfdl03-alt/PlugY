#!/usr/bin/env python3
"""
Simulate the Diablo II Classic big stash patches of PlugY/BigStash.cpp on real game files,
with the same expected-value checks as memt_byte / memt_dword / memt_ref4 / memc_ref4
(PlugY/ModifMemory.cpp). A failed check here is the "BYTE/DWORD ... wanted but ... found"
error box PlugY shows at startup when ActiveCheckMemory=1.

The game files are not in the repository (copyrighted) : pass the ones you have.

  tools/simulate_patches.py --109b D2Client.dll --109d D2Client.dll --110 D2Client.dll --111 D2Client.dll \
      --111b D2Client.dll --112 D2Client.dll --113c D2Client.dll --113d D2Client.dll --114d Game.exe

It also checks that the offsets below are the ones written in the R8(...) lines of
PlugY/BigStash.cpp (all the 9 columns), so update both together.
"""
import argparse, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Per version : image base, IsLodGame RVA (Commons/D2Funcs.h, 0 when there is no 'lod' or 'mouse' patch), patches.
# Patch kinds : ('push104', rva) existing background path patch, ('cref', rva, target_rva) memc_ref4,
# ('upd', rva, bank_rva) byte A1->E8 + memt_ref4(bank image), ('lod', rva) memt_dword(IsLodGame),
# ('mouse', rva) byte 8B->E8 + memt_ref4(0D|IsLodGame<<8) + byte(IsLodGame>>24).
VERSIONS = {
    '109b': dict(base=0x6FAA0000, islod=0, patches=[
        ('push104', 0x45B1C), ('cref', 0x45B06, 0xC093A)] +
        [('cref', rva, 0xBA00) for rva in (0x38ECF, 0x38FF1, 0x42D46, 0x45052, 0x42D9E, 0x45341, 0x4577E)]),
    '109d': dict(base=0x6FAA0000, islod=0, patches=[
        ('push104', 0x45B1C), ('cref', 0x45B06, 0xBFCBA)] +
        [('cref', rva, 0xB9F0) for rva in (0x38ECF, 0x38FF1, 0x42D46, 0x45052, 0x42D9E, 0x45341, 0x4577E)]),
    '110': dict(base=0x6FAA0000, islod=0, patches=[
        ('push104', 0x4C61C), ('cref', 0x4C606, 0xCA6DA)] +
        [('cref', rva, 0xC080) for rva in (0x3F1FE, 0x3F327, 0x498F8, 0x4BB62, 0x49950, 0x4BE4E, 0x4C27B)]),
    '111': dict(base=0x6FAB0000, islod=0xFB3F4, patches=[
        ('push104', 0xA643C), ('cref', 0xA6426, 0xBEC4), ('upd', 0xB0E85, 0x11BA7C),
        ('lod', 0xB0E94), ('lod', 0xB0FAB), ('lod', 0xA6AAC), ('mouse', 0xA4CA0)]),
    '111b': dict(base=0x6FAB0000, islod=0x11A2F4, patches=[
        ('push104', 0x749BC), ('cref', 0x749A6, 0xBED0), ('upd', 0x7DE05, 0x11BC18),
        ('lod', 0x7DE14), ('lod', 0x7DF2B), ('lod', 0x7164C), ('mouse', 0x6FE60)]),
    '112': dict(base=0x6FAB0000, islod=0x10330C, patches=[
        ('push104', 0xA9D7C), ('cref', 0xA9D66, 0xBF92), ('upd', 0xB34D5, 0x11BA38),
        ('lod', 0xB34E4), ('lod', 0xB35FB), ('lod', 0xA69FC), ('mouse', 0xA5530)]),
    '113c': dict(base=0x6FAB0000, islod=0x119854, patches=[
        ('push104', 0x8CC1C), ('cref', 0x8CC06, 0xBF8C), ('upd', 0x998D5, 0x11BC74),
        ('lod', 0x998E4), ('lod', 0x999FB), ('lod', 0x8D14C), ('mouse', 0x8B6B0)]),
    '113d': dict(base=0x6FAB0000, islod=0x1087B4, patches=[
        ('push104', 0x943FC), ('cref', 0x943E6, 0xBEA0), ('upd', 0x9DCA5, 0x11CB64),
        ('lod', 0x9DCB4), ('lod', 0x9DDCB), ('lod', 0x9119C), ('mouse', 0x8FA30)]),
    '114d': dict(base=0x400000, islod=0x3A04F4, patches=[
        ('push104', 0x89EB5), ('cref', 0x89E9D, 0x8F20),
        ('cref', 0x8F19C, 0x4DCC0), ('cref', 0x8F2B1, 0x4DCC0), ('cref', 0x8820D, 0x4DCC0), ('cref', 0x89921, 0x4DCC0)]),
}
COLUMN = {'109b': 0, '109d': 1, '110': 2, '111': 3, '111b': 4, '112': 5, '113c': 6, '113d': 7, '114d': 8}  # index in the 9 R8 columns


def source_offsets():
    """Offsets written in the R8(D2Client, ...) lines of BigStash.cpp, per version."""
    text = open(os.path.join(ROOT, 'PlugY', 'BigStash.cpp'), encoding='latin-1').read()
    found = {v: set() for v in COLUMN}
    for m in re.finditer(r'R8\(D2Client,([^)]*)\)', text):
        cols = [c.strip() for c in m.group(1).split(',')]
        for v, i in COLUMN.items():
            if cols[i] != '0000':
                found[v].add(int(cols[i], 16))
    return found


class Image:
    def __init__(self, path, base):
        self.d = bytearray(open(path, 'rb').read())  # PE sections of these files : file offset == RVA
        self.base = base
        self.pos = 0
        self.fails = 0

    def check(self, kind, old, found):
        ok = old == found
        self.fails += not ok
        print('  %-5s %08X wanted %08X found %08X  %s' % (kind, self.base + self.pos, old, found, 'OK' if ok else 'FAIL'))

    def dword(self):
        return struct.unpack('<I', self.d[self.pos:self.pos + 4])[0]

    def byte(self, old):
        self.check('BYTE', old, self.d[self.pos]); self.pos += 1

    def ref4(self, old):
        self.check('DWORD', old & 0xFFFFFFFF, self.dword()); self.pos += 4

    def cref4(self, old):
        self.check('CALL', old, (self.dword() + self.base + self.pos + 4) & 0xFFFFFFFF); self.pos += 4


def simulate(version, path, written):
    cfg = VERSIONS[version]
    img = Image(path, cfg['base'])
    base, islod = cfg['base'], cfg['base'] + cfg['islod']
    print('%s : %s' % (version, path))
    for p in cfg['patches']:
        kind, rva = p[0], p[1]
        if kind != 'push104' and rva not in written:
            print('  offset %X is not in the R8 lines of BigStash.cpp' % rva)
            img.fails += 1
        img.pos = rva
        if kind == 'push104':
            img.byte(0x68); img.ref4(0x104)
        elif kind == 'cref':
            img.cref4(base + p[2])
        elif kind == 'upd':
            img.byte(0xA1); img.ref4(base + p[2])
        elif kind == 'lod':
            img.ref4(islod)
        elif kind == 'mouse':
            img.byte(0x8B); img.ref4(0x0D | (islod << 8)); img.byte((islod >> 24) & 0xFF)
    return img.fails


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--109b', dest='v109b', help='D2Client.dll of 1.09b')
    parser.add_argument('--109d', dest='v109d', help='D2Client.dll of 1.09d')
    parser.add_argument('--110', dest='v110', help='D2Client.dll of 1.10')
    parser.add_argument('--111', dest='v111', help='D2Client.dll of 1.11')
    parser.add_argument('--111b', dest='v111b', help='D2Client.dll of 1.11b')
    parser.add_argument('--112', dest='v112', help='D2Client.dll of 1.12')
    parser.add_argument('--113c', dest='v113c', help='D2Client.dll of 1.13c')
    parser.add_argument('--113d', dest='v113d', help='D2Client.dll of 1.13d')
    parser.add_argument('--114d', dest='v114d', help='Game.exe of 1.14d')
    args = parser.parse_args()
    files = {'109b': args.v109b, '109d': args.v109d, '110': args.v110, '111': args.v111, '111b': args.v111b,
             '112': args.v112, '113c': args.v113c, '113d': args.v113d, '114d': args.v114d}
    if not any(files.values()):
        parser.error('give at least one game file')
    written = source_offsets()
    fails = sum(simulate(v, f, written[v]) for v, f in files.items() if f)
    print('FAILS: %d' % fails)
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
