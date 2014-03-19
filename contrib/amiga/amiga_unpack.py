#!/usr/bin/env python

# Some of the Amiga data files are run length encoded and need to be unpacked.
# Not all files are packed, and some are both coded and packed.

import sys

if __name__ == '__main__':
    if len(sys.argv) > 1:
        print 'Unpack Settlers Amiga data file from stdin to stdout'
        sys.exit(1)

    escape = sys.stdin.read(1)

    while True:
        c = sys.stdin.read(1)
        if c == '':
            break

        if c != escape:
            sys.stdout.write(c)
        else:
            n = ord(sys.stdin.read(1)) + 1
            c = sys.stdin.read(1)
            sys.stdout.write(n*c)
