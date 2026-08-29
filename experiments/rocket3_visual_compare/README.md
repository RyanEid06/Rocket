# Rocket 3 provisional visual comparator

This standalone C++20 experiment compares equal-sized raw RGBA buffers. It is
an explicitly provisional F27 kernel and is not part of Rocket's top-level
CMake project, renderer, image I/O, golden-reference, or CI surface.

Configure and test it with generated state below the packet boundary:

```powershell
cmake -S experiments/rocket3_visual_compare -B out/rocket3-provisional/wp07/build -G Ninja
cmake --build out/rocket3-provisional/wp07/build
ctest --test-dir out/rocket3-provisional/wp07/build --output-on-failure
```
