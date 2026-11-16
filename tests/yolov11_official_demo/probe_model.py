#!/usr/bin/env python3
"""Diagnose a YOLO bmodel's class output with the box's Python sail.

Usage (on the box):
  python3 tests/yolov11_official_demo/probe_model.py \
      --model /data/CT/project/ai/models/BM1684X/yolov11s_int8_1b_back_reflectivejacket_1.bmodel \
      --image tests/yolov11_official_demo/data/images/reflective_jacket_1280x720.jpg

For the FP16 counterpart add --norm 255 (official /255 normalization).
For the INT8 custom model both 1 and 255 produce all-zero class scores (KI-001).
"""
import argparse
import sys

import cv2
import numpy as np

try:
    import sophon.sail as sail
except ImportError as exc:
    print(f"FAIL: cannot import sophon.sail: {exc}")
    sys.exit(2)


def letterbox(image, size=640):
    h, w = image.shape[:2]
    r = min(size / w, size / h)
    nw, nh = int(round(w * r)), int(round(h * r))
    resized = cv2.resize(image, (nw, nh))
    canvas = np.full((size, size, 3), 114, dtype=np.uint8)
    x, y = (size - nw) // 2, (size - nh) // 2
    canvas[y:y + nh, x:x + nw] = resized
    return canvas


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--norm", type=float, default=1.0,
                        help="input divisor: 1 for 0-255 bmodels, 255 for /255 bmodels")
    parser.add_argument("--conf", type=float, default=0.25)
    args = parser.parse_args()

    image = cv2.imread(args.image)
    if image is None:
        print(f"FAIL: cannot read {args.image}")
        return 2
    canvas = letterbox(image)
    # YOLO models use RGB; NCHW float32.
    rgb = cv2.cvtColor(canvas, cv2.COLOR_BGR2RGB).transpose(2, 0, 1)[None].astype(np.float32)
    blob = rgb / args.norm

    engine = sail.Engine(args.model, 0, sail.IOMode.SYSIO)
    graph = engine.get_graph_names()[0]
    in_name = engine.get_input_names(graph)[0]
    out_name = engine.get_output_names(graph)[0]
    raw = engine.process(graph, {in_name: blob})[out_name][0]
    if raw.shape[0] < raw.shape[1]:
        raw = raw.T
    cls = raw[:, 4:]
    print(f"model={args.model}")
    print(f"output_shape={raw.shape} class_max={cls.max():.6f} "
          f"class_nonzero={np.count_nonzero(cls)} anchors>conf={int((cls.max(axis=1) > args.conf).sum())}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
