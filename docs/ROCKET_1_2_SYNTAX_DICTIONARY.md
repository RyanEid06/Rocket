# Rocket 1.2 Development Syntax Dictionary

This dictionary records additive syntax implemented during Rocket 1.2
development. Rocket 1.0 and 1.1 programs keep their existing meaning.

## Impl blocks

```text
impl-declaration := "impl" type-parameters? type-name ":" NEWLINE INDENT
                    impl-member+ DEDENT
impl-member      := "pub"? function-declaration
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

Traits, generic constraints, function types, lambdas, closures, and
user-defined iterator syntax are not defined by this initial Phase 12 slice.
