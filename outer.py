import numpy as np
from PIL import Image
arr = np.fromfile('out.raw', dtype=np.uint8).reshape(235, 215)
img = Image.fromarray(arr)
img.save('outerchess.png')
print('Saved outerchess.png')
