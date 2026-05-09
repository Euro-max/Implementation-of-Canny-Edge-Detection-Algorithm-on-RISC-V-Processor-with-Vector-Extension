Hey team 👋

I've pushed my work to the new branch: The_GOAT

Here's what I did:

✅ Gaussian Blur (gaussian.h / gaussian.ipp)
- 5x5 kernel with correct sum of 273
- Zero-padding boundary handling
- Template design (T_in, T_out, T_acc)

✅ Magnitude (magnitude.h / magnitude.cpp)
- L1 norm: |Gx| + |Gy|
- L2 norm: sqrt(Gx² + Gy²)
- Two-pass normalization to [0, 255]

✅ Unit Tests (tests/ folder)
- test_gaussian.cpp → 4/4 passing
- test_magnitude.cpp → 4/4 passing
- test_sobel.cpp → 4/4 passing
- test_direction.cpp → 4/4 passing
- test_image_io.cpp → 4/4 passing
- Total: 20/20 tests passing ✅

✅ Full pipeline tested natively on a real image (GOAT.jpg 720x900) and produced correct edge detection output

✅ Makefile updated with:
- make test_gaussian
- make test_magnitude
- make test_sobel
- make test_direction
- make test_image_io
- make test_all (runs everything at once)

Please pull from The_GOAT branch and let me know if anything conflicts with your work!

Adham 🐐
