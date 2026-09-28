# Rocket SDK roadmap

This roadmap uses feature names so readers can understand each milestone
without internal work-packet IDs. It summarizes the Rocket source at master
commit 1f6ba76f16f3246095d5d573c28d825d8b9367e3, packaged here for Windows
x64, Linux x64, Linux ARM64, and macOS Apple Silicon ARM64. The compiler still
identifies itself as Rocket 3.0.0.

| Milestone | Current state |
| --- | --- |
| Native game runtime foundation | Integrated in master and included in this SDK. |
| Textures, canvases, and display control | Integrated in master and included in this SDK. |
| Blending, shaders, and post-processing | Integrated in master and included in this SDK. |
| Audio and streamed music | Integrated in master and included in this SDK. |
| Managed game assets | The bundled rocket.assets module is included. |
| Rendered game UI | The bundled rocket.ui.render module is included. |
| Scroll2Roll readiness | Windows Debug and Release suites passed 307/307 each in the repository's local acceptance record. Cross-platform release evidence remains the final gate described there. |

The source roadmap and evidence are on master:

- [Rocket 3.5 implementation roadmap](https://github.com/RyanEid06/Rocket/blob/master/docs/ROCKET_3_5_ROADMAP_IMPLEMENTATION.md)
- [Scroll2Roll readiness evidence](https://github.com/RyanEid06/Rocket/blob/master/docs/ROCKET_3_5_WP7_EVIDENCE.md)
- [Rocket 3.5 public API inventory](https://github.com/RyanEid06/Rocket/blob/master/docs/ROCKET_3_5_API_INVENTORY.md)

## Distribution steps

1. Keep this SDK synchronized with a verified Rocket master commit.
2. Run bootstrap, package relocation, and checksum checks for each supported
   platform before listing a package as available.
3. Publish the verified SDK branch or archive, then update the website's
   download, version, source commit, and checksum information to match it.
4. Recheck the website's roadmap and SDK links after each Rocket release.

This branch includes four ready-to-use platform packages from the same source
commit. The three archive packages retain the published release artifacts and
their verified SHA-256 hashes in `SHA256SUMS.txt`; follow each included
`PACKAGE.md` for platform setup requirements.
