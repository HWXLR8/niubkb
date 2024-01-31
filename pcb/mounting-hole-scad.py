#!/usr/bin/env python3

import pcbnew

pcb = pcbnew.LoadBoard("niu-merged.kicad_pcb")

for m in pcb.GetFootprints():
    c = m.GetCenter()
    if m.GetValue() == "MountingHole_3.2mm_M3":
        print("mount(%s, %s);" % (c.x/1000000, c.y/1000000))
