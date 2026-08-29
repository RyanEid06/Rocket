# Rocket 3 provisional visual comparator and evidence schemas

This standalone C++20 experiment compares equal-sized raw RGBA buffers and
defines versioned, value-only evidence schemas for synthetic performance and
golden records. It is explicitly provisional and is not part of Rocket's
top-level CMake project, renderer, image I/O, final budgets, golden-reference,
or CI surface. The schemas record measured thresholds; they do not establish
final performance budgets or permit automatic golden updates.

Configure and test it with generated state below the packet boundary:

```powershell
cmake -S experiments/rocket3_visual_compare -B out/rocket3-provisional/wp08/build -G Ninja
cmake --build out/rocket3-provisional/wp08/build
ctest --test-dir out/rocket3-provisional/wp08/build --output-on-failure
```
