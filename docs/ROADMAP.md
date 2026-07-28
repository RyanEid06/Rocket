# Implementation Roadmap

## Completed in milestone 0.1

- Charter, syntax draft, examples, and decision journal
- CMake compiler/test foundation
- Indentation-aware lexer with source locations
- Parser and AST for the first procedural subset
- Local type inference, scopes, calls, and basic type checking
- Temporary native bootstrap backend and assembly inspection command

## Next: milestone 0.2

- Assignment with enforced mutability
- `for` loops and loop control
- Character and floating-point types
- Full control-flow return analysis and improved error recovery
- Golden diagnostic and command-line tests

## Then

Implement arrays, structs, enums, pattern matching, `Option`, `Result`, modules, reference-counted runtime values, LLVM IR generation, native object emission, standard-library I/O, benchmarks, and selected self-hosted frontend components in that order.
