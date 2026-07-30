# Rocket Diagnostic Catalog 1.0

Compiler diagnostics use the stable shape:

```text
path\file.rocket:12:9: error[R4002]: undefined name 'total'
```

For source diagnostics, the path, one-based line, one-based column, severity, code, and message are all
machine-readable. Messages may become clearer without changing the code's
category. Tools should match the complete `Rdddd` code rather than message text.
Manifest and tooling errors without a source position use
`rocketc: error[R5001]: ...` or `rocketc: error[R5002]: ...`.

| Code | Category | Typical cause |
| --- | --- | --- |
| `R1001` | Lexical | Invalid character, literal, or escape |
| `R1002` | Indentation | Tab indentation or a non-four-space block |
| `R2001` | Syntax | Missing or unexpected grammar token |
| `R3001` | Module not found | Imported source cannot be read |
| `R3002` | Import cycle | Recursive source-module dependency |
| `R3003` | Visibility | Private declaration used across modules |
| `R3004` | Import alias | Two imports claim the same local alias |
| `R4001` | Type/semantic | Type mismatch, invalid operation or mutation, unsupported native ABI type, or extern call outside `unsafe:` |
| `R4002` | Name resolution | Undefined value, function, or constructor |
| `R4003` | Control flow | Invalid loop control, entry point, or return path |
| `R4004` | Pattern match | Invalid, duplicate, or non-exhaustive cases |
| `R4005` | Arity | Wrong number of call or constructor arguments |
| `R5001` | Manifest | Invalid `rocket.toml` metadata, output kind, native input, or contained path |
| `R5002` | Tooling | Formatter, test-runner, binding/header generation, linker, or artifact workflow failure |
| `R9001` | Internal compiler | Verified compiler invariant failed |

Runtime failures remain a separate `rocket runtime error:` stream and exit with
status 101. They do not pretend to have source positions once native code is
running. Expected file, parsing, and process failures in the standard library
are `Result` values rather than diagnostics.

The compiler test suite fixes golden output for representative errors and
asserts catalog identities across lexical, syntax, name, control-flow, match,
arity, manifest, and internal categories. The VS Code extension's `$rocket`
problem matcher consumes this format.
