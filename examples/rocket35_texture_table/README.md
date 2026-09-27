# Rocket 3.5 texture-backed casino table

This WP2 example loads the checked-in PNG artwork in `assets/`, renders the
cards and chips from atlases into an 800×450 logical canvas, presents it in a
resizable high-DPI window, and saves `casino-table.png` in its working
directory. The Rocket source imports only the canonical graphics and safe
Raylib modules. `make_assets.py` is the offline, repeatable artwork authoring
step; the application never draws artwork pixel by pixel.

Run the package from this directory with `rocketc run .` after installing or
activating the Rocket native SDK. The window appears for one frame and closes
after the screenshot is saved. The CTest test `rocket35_texture_table_native`
runs it from an isolated working directory with copies of the package assets
and verifies two identical captures.
