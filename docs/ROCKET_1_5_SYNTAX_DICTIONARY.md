# Rocket 1.5 Standard-Library Usage Dictionary

Rocket 1.5 introduces no new grammar. Programs use the existing import,
`Option`, `Result`, postfix `?`, struct-field, collection, and pattern-matching
syntax with the expanded standard library.

```rocket
import std.binary
import std.crypto
import std.file
import std.regex
import std.stream
import std.unicode

fn digest_file(path: String) -> Result[String, String]:
    let reader = stream.open_reader(path, 8192)?
    let bytes = stream.read(reader, 67108864)?
    stream.close_reader(reader)?
    return crypto.sha256(bytes)

fn main() -> Int:
    match regex.is_match("^[a-z]+$", unicode.normalize_nfc("rocket")?):
        case Ok(matches):
            if matches:
                print("valid")
                return 0
            return 1
        case Err(error):
            print(error)
            return 2
```

Additional ordinary imports are `std.archive`, `std.cli`, `std.compression`,
`std.config`, `std.datetime`, `std.http`, `std.log`, `std.net`, `std.sqlite`, and
`std.testing`. Public calls that can fail return `Result`; optional lookups use
`Option`; acquired streams, sockets, listeners, and databases must be closed.
Precise signatures, bounds, timeout behavior, and security policy are defined in
`STDLIB.md` and `RELEASE_1_5.md`.
