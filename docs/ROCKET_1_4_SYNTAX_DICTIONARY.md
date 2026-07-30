# Rocket 1.4 Graphics and Audio Usage Dictionary

Rocket 1.4 introduces no new grammar. It uses the Rocket 1.3 `unsafe`, native
declaration, callback, manifest, and generation spellings only inside the
reviewed raylib adapter package. Ordinary applications import the safe module:

```rocket
import src.rocket_raylib

match rocket_raylib.open_window(800, 450, "Rocket"):
    case Ok(window):
        match rocket_raylib.begin_frame(window):
            case Ok(frame):
                let cleared = rocket_raylib.clear(frame, rocket_raylib.black())
                let ended = rocket_raylib.end_frame(frame)
            case Err(error):
                print(error)
        let closed = rocket_raylib.close_window(window)
    case Err(error):
        print(error)
```

`Window`, `Frame`, `Texture`, `Font`, `AudioDevice`, and `Sound` are safe Rocket
resource tokens, not native pointers. Operations that create, destroy, or can
fail return `Result`. Input queries and timing return primitive values after the
wrapper validates their owning device. All ownership rules are specified in
`RELEASE_1_4.md` and the reference package README.
