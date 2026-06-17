import numpy as np
import matplotlib.pyplot as plt
import sys

width, height = 612, 437

try:
    # Read raw binary data into numpy arrays
    img_in = np.fromfile('test_input.raw', dtype=np.uint8).reshape((height, width))
    img_out = np.fromfile('out.raw', dtype=np.uint8).reshape((height, width))

    # Display side-by-side
    fig, ax = plt.subplots(1, 2, figsize=(12, 6))
    ax[0].imshow(img_in, cmap='gray')
    ax[0].set_title("Input Image")
    ax[0].axis('off')
    
    ax[1].imshow(img_out, cmap='gray')
    ax[1].set_title("Pipeline Output (Direction/Magnitude)")
    ax[1].axis('off')
    
    plt.show()
except Exception as e:
    print(f"Error loading images: {e}")
