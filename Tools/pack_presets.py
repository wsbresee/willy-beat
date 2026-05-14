#!/usr/bin/env python3
"""Pack all Presets/*.beat files into a single binary bundle.

Bundle format (all integers little-endian):
  uint32  magic   = 0x50425442  ('PBTB')
  uint32  version = 1
  uint32  count
  for each file:
    uint32  name_len          (no NUL terminator)
    name_len bytes            (UTF-8 file name without the .beat extension)
    uint32  data_len
    data_len bytes            (raw .beat file text as UTF-8)

Run: python3 Tools/pack_presets.py <presets-dir> <output.bin>
"""

import os
import struct
import sys


def main():
    if len(sys.argv) != 3:
        print("usage: pack_presets.py <presets-dir> <output.bin>", file=sys.stderr)
        sys.exit(2)

    presets_dir, out_path = sys.argv[1], sys.argv[2]

    entries = []
    for fname in sorted(os.listdir(presets_dir)):
        if not fname.lower().endswith(".beat"):
            continue
        data = open(os.path.join(presets_dir, fname), "rb").read()
        name = os.path.splitext(fname)[0].encode("utf-8")
        entries.append((name, data))

    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(out_path, "wb") as f:
        f.write(struct.pack("<IIII", 0x50425442, 1, len(entries), 0))  # magic, ver, count, pad
        for name, data in entries:
            f.write(struct.pack("<I", len(name)))
            f.write(name)
            f.write(struct.pack("<I", len(data)))
            f.write(data)

    print(f"packed {len(entries)} presets -> {out_path}")


if __name__ == "__main__":
    main()
