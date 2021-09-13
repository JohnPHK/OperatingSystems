#!/usr/bin/env python3

import sys
import re

markerfile = open(sys.argv[1])

line = markerfile.read()
line = line.strip()
markers = line.split(' ')

tracefile = open(sys.argv[2])

p = re.compile(r'\s*[IMSL]\s+([0-9a-fA-F]+),')
found = False

for line in tracefile:
    line = line.strip()
    m = p.match(line)

    if m:
        addr = '0x' + m.group(1)
        #print(addr)
        if not found:
            if addr == markers[0]:
                #print(addr + " " +  markers[0])
                found = True
        else:
            if addr == markers[1]:
                #print(addr + " " + markers[1])
                break
            else:
                print(line)
