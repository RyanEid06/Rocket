# Rocket 1.3 Native Interoperability Syntax Dictionary

This dictionary records the additive native-interoperability syntax introduced
in Rocket 1.3. Rocket 1.0-1.2 programs keep their existing meaning.

`unsafe`, `extern`, `export`, `opaque`, and `callback` are contextual spellings,
not globally reserved words. They remain valid identifiers outside the grammar
positions documented below.

## Unsafe blocks

```text
unsafe-statement := "unsafe" ":" NEWLINE INDENT statement+ DEDENT
```

Every call to an imported native function must be lexically contained by an
`unsafe:` block. The marker acknowledges the foreign contract; it does not
disable Rocket type checking, mutability checking, control-flow validation, or
ARC rules.

```rocket
unsafe:
    let handle = counter_create(42)
    counter_destroy(handle)
```

## Native declarations

```text
extern-function := "pub"? "extern" "fn" IDENTIFIER
                   "(" parameters? ")" "->" native-type NEWLINE
extern-constant := "pub"? "extern" "const" IDENTIFIER ":" primitive-type
                   "=" literal NEWLINE
extern-opaque   := "pub"? "extern" "opaque" IDENTIFIER NEWLINE
extern-struct   := "pub"? "extern" "struct" IDENTIFIER ":" NEWLINE INDENT
                   native-field+ DEDENT
native-field    := IDENTIFIER ":" native-type NEWLINE
extern-callback := "pub"? "extern" "callback" IDENTIFIER
                   "(" parameters? ")" "->" native-type NEWLINE
export-function := "pub"? "export" "fn" IDENTIFIER
                   "(" parameters? ")" "->" native-type ":"
                   NEWLINE INDENT statement+ DEDENT
native-type     := primitive-type | "Pointer" "[" native-type "]"
                   | opaque-name | callback-name
```

`extern fn` names an imported C symbol. `export fn` defines a Rocket function
and emits an unmangled C wrapper with the same name. `extern const` is a
compile-time primitive literal. Native declarations and exports cannot be
generic.

`extern opaque` is an incomplete C type represented as a pointer handle.
`extern struct` records field order and types for generated bindings and
headers; values of that type cross calls only as `Pointer[StructName]`.
`Pointer[Unit]` maps to `void*`.

```rocket
pub extern const API_LEVEL: Int = 13
pub extern opaque NativeCounter
pub extern struct NativePoint:
    x: Int
    y: Int
pub extern callback NativeUnary(value: Int) -> Int
pub extern fn counter_create(value: Int) -> NativeCounter
pub extern fn point_sum(point: Pointer[NativePoint]) -> Int
pub extern fn apply(action: NativeUnary, value: Int) -> Int

export fn rocket_double(value: Int) -> Int:
    return value * 2
```

A callback argument must be an exactly matching top-level noncapturing function.
Callbacks are synchronous and non-storing. Native pointers and opaque handles
are non-ARC and are released only through the C API documented for them.

## Package and generation spellings

Native artifact selection and linker inputs are manifest data, not source
syntax:

```toml
[build]
kind = "static-library"
name = "rocket_math"

[native.windows-x64]
libraries = "vendor.lib;support.lib"
library-search = "native/lib"
headers = "native/vendor.h"
```

Semicolon-separated lists preserve source order. `rocketc bind <header.h>`
prints deterministic Rocket declarations, and `--output <file>` writes them.
`rocketc emit-header <package>` similarly prints a deterministic C consumer
header or writes it with `--output <file>`.
