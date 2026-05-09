from PIL import Image
import numpy as np

img = Image.open('carlsen.jpg').convert('L')  # Convert to grayscale
arr = np.array(img)
arr.tofile('carlsen.raw')
print(f'Converted! Size: {arr.shape[1]}x{arr.shape[0]}')
