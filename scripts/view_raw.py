#!/usr/bin/env python3
# Converts RAW grayscale file back to viewable PNG
# Also creates a side-by-side comparison of input vs output
# Usage: python3 view_raw.py input.raw output.raw width height

import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

def view_raw(input_raw, output_raw, width, height):

    # Load both RAW files as numpy arrays
    inp = np.fromfile(input_raw, dtype=np.uint8).reshape(height, width)
    out = np.fromfile(output_raw, dtype=np.uint8).reshape(height, width)

    # Print some statistics
    print(f"\n--- Input Image ---")
    print(f"  Size: {width}x{height}")
    print(f"  Min: {inp.min()}, Max: {inp.max()}, Mean: {inp.mean():.1f}")

    print(f"\n--- Edge Output ---")
    print(f"  Min: {out.min()}, Max: {out.max()}, Mean: {out.mean():.1f}")
    nonzero = np.count_nonzero(out)
    print(f"  Non-zero (edge) pixels: {nonzero} ({100*nonzero/(width*height):.1f}%)")

    # Save input as PNG
    Image.fromarray(inp).save("images/input_view.png")
    print(f"\nSaved: images/input_view.png")

    # Save output as PNG
    Image.fromarray(out).save("images/output_edges.png")
    print(f"Saved: images/output_edges.png")

    # Create side-by-side comparison image
    combined_w = width * 2 + 10   # 10px gap between images
    combined   = Image.new('L', (combined_w, height), color=128)
    combined.paste(Image.fromarray(inp), (0, 0))
    combined.paste(Image.fromarray(out), (width + 10, 0))
    combined.save("images/comparison.png")
    print(f"Saved: images/comparison.png  (side-by-side comparison)")

    print(f"\nTo view: open images/comparison.png in Windows Explorer")
    print(f"Windows path: \\\\wsl.localhost\\Ubuntu\\home\\{__import__('os').environ.get('USER','')}\\canny-edge\\images\\comparison.png")

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python3 view_raw.py input.raw output.raw width height")
        sys.exit(1)

    view_raw(sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]))
