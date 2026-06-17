#!/usr/bin/env python3
# Converts any image (JPG/PNG/BMP) to RAW grayscale format
# Usage: python3 img_to_raw.py input.jpg output.raw [width] [height]

import sys
from PIL import Image

def convert_to_raw(input_path, output_path, target_w=None, target_h=None):

    # Open the image (works with JPG, PNG, BMP, anything Pillow supports)
    img = Image.open(input_path)

    # Convert to grayscale (L = luminance = single channel 0-255)
    img = img.convert('L')

    # Resize if width/height were specified
    if target_w and target_h:
        img = img.resize((target_w, target_h), Image.LANCZOS)

    w, h = img.size
    print(f"Image size: {w}x{h}")

    # Save as raw bytes (no header, just pixels)
    raw_bytes = img.tobytes()
    with open(output_path, 'wb') as f:
        f.write(raw_bytes)

    print(f"Saved RAW: {output_path} ({len(raw_bytes)} bytes)")
    return w, h

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 img_to_raw.py input.jpg output.raw [width] [height]")
        sys.exit(1)

    inp  = sys.argv[1]
    out  = sys.argv[2]
    w    = int(sys.argv[3]) if len(sys.argv) > 3 else None
    h    = int(sys.argv[4]) if len(sys.argv) > 4 else None

    final_w, final_h = convert_to_raw(inp, out, w, h)
    # Print dimensions so Makefile can use them
    print(f"DIMENSIONS:{final_w}:{final_h}")
