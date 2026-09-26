# Rocket 3.5 WP4 casino audio scene

This Scroll2Roll-style table uses only supported stdlib imports. It loads
repository-owned felt, card, chip, and audio assets, then renders at an
800x450 logical size through the canonical texture, blend, shader, and canvas
APIs. The music stream plays continuously while the scene updates it every
frame. Two different interface/game effects start during music playback.

Run `rocketc run .` here with the Rocket native SDK installed and an available
display and audio device. The scene writes `casino-audio-before.png` and
`casino-audio-resized.png` in the working directory. It releases the sound
effects and music stream before closing the audio device. Its CMake acceptance
test builds and runs the ordinary package and checks both images. The
deterministic native and package fixtures use the adapter's established test
mode on headless hosts.

See [the audio guide](../../docs/ROCKET_3_5_AUDIO.md) for the supported
lifecycles, ownership rule, and game-loop update pattern.
