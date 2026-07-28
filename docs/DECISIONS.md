# Design Decision Journal

## D001 - LLVM ahead-of-time backend

**Accepted.** LLVM provides optimized native code while keeping the project focused on language semantics. The development toolchain pins LLVM 22.1.6. The first executable slice continues to use a clearly isolated C++ bootstrap backend until scalar LLVM IR lowering is implemented. The frontend must not depend on that backend.

## D002 - Inferred static typing

**Accepted.** Local inference preserves concise code while compile-time checking supports predictable performance and beginner-friendly diagnostics.

## D003 - Indentation-delimited blocks

**Accepted.** This favors visual clarity. The lexer, not the parser, converts indentation into explicit `Indent` and `Dedent` tokens.

## D004 - Reference counting

**Accepted.** Reference counting offers deterministic cleanup and a tractable first runtime. Cycles remain a documented limitation for version 1.

## D005 - Explicit failure and absence

**Accepted.** `Result[T, E]` and `Option[T]` replace exceptions and universal null. Their compiler implementation follows algebraic data types.

## D006 - Scalar control-flow slice

**Accepted.** Assignment is restricted to existing `var` bindings and must preserve their inferred type. Integer `for` ranges use `start..end` with an exclusive end; loop variables are immutable. `break` and `continue` are loop-only statements. `Float` is a distinct scalar type with no implicit conversion from `Int`, and `Char` is a single escaped byte literal in the stage0 bootstrap implementation.
