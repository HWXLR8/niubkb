#!/usr/bin/env python3

import pcbnew

pcb = pcbnew.LoadBoard("niu-merged.kicad_pcb")

offset_points = []

for m in pcb.GetFootprints():
    center = m.GetCenter()
    if m.GetValue() == "MountingHole_3.2mm_M3":
        offset_points.append([center.x, center.y])

# calculate centroid
cx = 0
cy = 0
for point in offset_points:
    cx += point[0]
    cy += point[1]
centroid = [cx/len(offset_points), cy/len(offset_points)]

# move points relative to origin, and print
for point in offset_points:
    px = point[0] - centroid[0]
    py = point[1] - centroid[1]
    print("mount(%s, %s);" % (px/1000000, py/1000000))
