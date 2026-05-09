from PIL import Image
import numpy as np

img = Image.open('GOAT.jpg').convert('L')
arr = np.array(img)
arr.tofile('test_input.raw')
print(f'Size: {arr.shape[1]}x{arr.shape[0]}')
