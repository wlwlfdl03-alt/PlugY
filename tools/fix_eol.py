#!/usr/bin/env python3
"""
Restore the line endings of the files modified in the working tree.
Most sources are CRLF; some files (README.md, .gitignore, workflows) are LF and some have no
final newline. Each modified file gets the line ending and final newline of its HEAD version.
New files are left as they are.

  tools/fix_eol.py
"""
import os, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def git(*args):
    return subprocess.run(['git', '-C', ROOT] + list(args), capture_output=True, check=False)


for name in git('diff', '--name-only').stdout.decode().split():
    orig = git('show', 'HEAD:' + name).stdout
    path = os.path.join(ROOT, name)
    if not orig or not os.path.exists(path):
        continue
    data = open(path, 'rb').read().replace(b'\r\n', b'\n')
    if b'\r\n' in orig:
        data = data.replace(b'\n', b'\r\n')
    if not orig.endswith(b'\n'):
        data = data.rstrip(b'\r\n')
    elif not data.endswith(b'\n'):
        data += b'\r\n' if b'\r\n' in orig else b'\n'
    open(path, 'wb').write(data)
    print('fixed', name)
