#!/usr/bin/env python3
"""Assert that the official YOLOv11 demo produced usable detections."""
import glob
import json
import os
import sys

test_dir = os.path.dirname(os.path.abspath(__file__))
files = sorted(glob.glob(os.path.join(test_dir, "results", "*.json")))
if not files:
    print("FAIL: no result JSON under results/")
    sys.exit(1)

images = 0
total = 0
per_class = {}
for path in files:
    with open(path, encoding="utf-8") as f:
        rows = json.load(f)
    images += len(rows)
    for row in rows:
        for box in row.get("bboxes", []):
            total += 1
            cls = box.get("category_id")
            per_class[cls] = per_class.get(cls, 0) + 1

print(f"result_json={os.path.basename(files[0])}")
print(f"images={images} detections={total} per_class={per_class}")
if total == 0:
    print("FAIL: demo produced zero detections")
    sys.exit(1)
print("PASS: demo produced detections")
