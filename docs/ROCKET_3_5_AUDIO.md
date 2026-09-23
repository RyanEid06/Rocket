# Rocket 3.5 audio and streamed music

Import `rocket.raylib.safe` for the supported game audio API. `AudioDevice`,
`Sound`, and `Music` are separate typed handles. Operations that can fail return
`Result[..., String]`; a missing file, unavailable device, bad value, or stale
handle is an error that the game can handle without terminating.

## Ownership and order

Open one audio device with `open_audio()`. Create sounds with
`load_sound(audio, path)` or `create_tone(audio, frequency, seconds)` and music
with `load_music(audio, path)`. Paths are resolved from the game's working
directory; use a package-root-based absolute path when launching from another
directory. A sound or music handle is owned by the audio device that created
it. **Unload every sound and music stream before `close_audio(audio)`.** A
close with live resources returns `raylib: owned resource is still live` and
leaves the device open, so cleanup can finish. Closing again, or using a
released handle, returns `raylib: stale or already released handle`.

Music is streamed rather than decoded into one short sound buffer. Call
`update_music(music)` **once each game-loop iteration while it is playing**.
An update before play, after stop, or while paused returns
`raylib: invalid lifecycle state`. Pause requires playing music; resume
requires paused music. `stop_music` is safe to call during cleanup, including
after an earlier stop. `music_playing` is false during pause and after stop.
`set_music_looping(music, true)` requests continuous playback. Sound effects
can play at the same time as music; separate sound handles can overlap.

## Normal game-loop pattern

This excerpt assumes `window`, `audio`, `music`, `click`, and `chip` were
created through successful `safe` calls. Handle each `Err` in the application
and keep the final cleanup order shown.

```rocket
import rocket.raylib.safe

let started = safe.play_music(music)
var running = true
while running:
    match safe.window_should_close(window):
        case Err(error):
            print(error)
            running = false
        case Ok(should_close):
            if should_close:
                running = false
            else:
                match safe.update_music(music):
                    case Err(error):
                        print(error)
                        running = false
                    case Ok(updated):
                        # Play effects in response to input or game events.
                        # safe.play_sound(click) and safe.play_sound(chip)
                        # may both run while music is streaming.
                        match safe.begin_frame(window):
                            case Err(error):
                                print(error)
                                running = false
                            case Ok(frame):
                                let cleared = safe.clear_background(frame, safe.color(12, 38, 34))
                                let ended = safe.end_frame(frame)

let stopped = safe.stop_music(music)
let unloaded_chip = safe.unload_sound(chip)
let unloaded_click = safe.unload_sound(click)
let unloaded_music = safe.unload_music(music)
let closed_audio = safe.close_audio(audio)
let closed_window = safe.close_window(window)
```

`sound_playing`, `music_playing`, and `audio_ready` return checked status
results. Sound volume and music volume accept finite values from 0.0 to 1.0.
Sound pitch accepts a positive multiplier up to 4.0. `play_sound` may be
called repeatedly on a loaded effect. Use `stop_sound` when an effect must
be cut short, then `unload_sound` when it is no longer needed.

The [WP4 casino scene](../examples/rocket35_audio/README.md) is a complete
working example. Its render loop updates the stream every frame and starts two
effects while the music is playing. The headless adapter and Rocket package
fixtures exercise the same lifetime contract without an audio device.
