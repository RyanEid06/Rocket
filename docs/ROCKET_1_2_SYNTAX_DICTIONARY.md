# Rocket 1.2 Syntax Dictionary

This dictionary records the additive syntax implemented in Rocket 1.2. Rocket
1.0 and 1.1 programs keep their existing meaning.

## Impl blocks

```text
impl-declaration := "impl" type-parameters? type-name ":" NEWLINE INDENT
                    impl-member+ DEDENT
impl-member      := "pub"? (function-declaration | associated-constant)
associated-constant := "const" IDENTIFIER ":" type-name "=" expression NEWLINE
```

`impl` is reserved. The owner must be a struct or enum in the same module.
Generic parameters are declared before the owner, for example
`impl[T] Box[T]:`.

An instance method has an explicit first parameter named `self`, whose type is
exactly the impl owner. A function without `self` is associated with the owner
and is called through the type:

```rocket
impl Counter:
    pub fn zero() -> Counter:
        return Counter(0)

    pub fn add(self: Counter, amount: Int) -> Counter:
        return Counter(self.value + amount)

let result = Counter.zero().add(2)
```

The dot-call form inserts the receiver as the first direct-call argument after
static typing. It does not imply mutation or dynamic dispatch. Public visibility
belongs to each member, not to the impl block.

## Standard-library dot calls

The implemented aliases cover String operations; Array algorithms and mutation;
Slice length; Map length/find/get/keys/values; and Set contains/values. Examples:

```rocket
values = values.append(42)
print(values.length())
print(" rocket ".trim())
print(String.from_int(42))
```

Module-function spellings such as `collections.append(values, 42)` remain valid
and resolve to the same intrinsic.

## Traits and constraints

```text
trait-declaration := "pub"? "trait" IDENTIFIER ":" NEWLINE INDENT
                     trait-method+ DEDENT
trait-method      := "fn" IDENTIFIER "(" parameters ")" "->" type-name NEWLINE
trait-impl        := "impl" type-name "for" type-name ":" NEWLINE INDENT
                     ("pub"? function-declaration)+ DEDENT
where-clause      := "where" IDENTIFIER ":" type-name
                     ("," IDENTIFIER ":" type-name)*
```

Trait methods begin with `self: Self`. Calls are statically selected; inherent
methods win and ambiguous trait implementations are diagnosed.

## Lambdas and closure values

```text
lambda-expression := "fn" "(" parameters? ")" "->" type-name
                     "=>" expression
```

Lambdas capture referenced locals by value in source order. Their concrete,
compiler-generated closure type can be passed through generics and called like
a function, including directly as `(fn(value: Int) -> Int => value)(42)`.
Parameter and result annotations may use type parameters from the enclosing
generic function; every specialization substitutes them before HIR. Captures
follow the normal ARC rules; no erased function type or dynamic dispatch is
introduced.

## User-defined iteration

`for item in value:` requires `iterator`, `has_next`, `value`, and `advance`
methods. `has_next` returns `Bool`; `advance` returns the same cursor type.
The protocol is persistent: advancing produces the next cursor value.

## Associated constants and parameters

`Owner.NAME` accesses a non-generic impl constant. It is evaluated as a direct
zero-argument call and has no mutable global storage. Parameters in Rocket 1.2
are positional-only, required, and fixed-arity. Generic compilation creates at
most 4,096 user specializations per program.
