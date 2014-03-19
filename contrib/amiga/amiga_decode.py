#!/usr/bin/env python

# Some of the Amiga data files are XOR coded and need to be decoded before
# the data can be read. Not all files are coded, and some are both coded and
# packed.

import sys

if __name__ == '__main__':
    if len(sys.argv) > 1:
        print 'Decode Settlers amiga data file from stdin to stdout'
        sys.exit(1)

    i = 0
    while True:
        c = sys.stdin.read(1)
        if c == '':
            break

        d = chr((ord(c) ^ i) & 0xff)
        i += 1
        sys.stdout.write(d)
