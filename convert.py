from PIL import Image

# Make sure to change 'my_photo.jpg' to the actual name of your image!
img = Image.open('GOAT.jpg').convert('L')

with open('custom_input.raw', 'wb') as f:
    f.write(img.tobytes())

print(f"Image Width: {img.width}")
print(f"Image Height: {img.height}")
