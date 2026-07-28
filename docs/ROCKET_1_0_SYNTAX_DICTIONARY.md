# Rocket 1.0 Syntax Dictionary

This is the practical, copyable reference for writing Rocket 1.0 programs. It
summarizes the frozen language specification, standard modules, package format,
and compiler commands. Where this guide says a feature is unavailable, do not
assume syntax from Python, C++, Java, Rust, or another language will work.

## 1. Smallest program

Every executable needs `fn main() -> Int`. Its return value is the process exit
code; use `0` for success.

```rocket
fn main() -> Int:
    print("Hello from Rocket")
    return 0
```

Rocket files use the `.rocket` extension.

## 2. Layout, identifiers, and comments

- Blocks start after `:` and are indented by exactly four-space multiples.
- Tabs are errors.
- Braces and semicolons are not used.
- Blank lines do not change indentation.
- `#` starts a line comment outside a literal.
- Identifiers start with a letter or `_`, followed by letters, digits, or `_`.
- Rocket is case-sensitive: `value`, `Value`, and `VALUE` are different names.

```rocket
# A comment
fn classify(value: Int) -> Int:
    if value > 0:
        return 1
    else:
        return 0
```

Reserved words:

```text
fn let var if else while for in break continue return
true false and or not struct enum match case pub import
```

Punctuation and operators:

```text
( ) [ ] : , -> + - * / . .. ? = == != < <= > >=
```

## 3. Literals

| Kind | Examples | Notes |
| --- | --- | --- |
| `Int` | `0`, `42`, `9223372036854775807` | Signed 64-bit; a negative value uses unary `-` |
| `Float` | `0.0`, `1.5`, `42.25` | IEEE-754 binary64; digits are required on both sides of `.` |
| `Bool` | `true`, `false` | No implicit conversion to or from numbers |
| `Char` | `'R'`, `'\n'` | Exactly one byte in Rocket 1.0 |
| `String` | `"Rocket"`, `"line\n"` | Immutable valid UTF-8 bytes |
| Array | `[1, 2, 3]` | All elements have exactly one type |

Character escapes are `\n`, `\r`, `\t`, `\\`, and `\'`. String escapes are
`\n`, `\r`, `\t`, `\\`, and `\"`. Rocket 1.0 has no multiline-string or
string-interpolation syntax.

There is no `null` literal and no standalone `Unit` literal.

## 4. Types

| Type | Meaning |
| --- | --- |
| `Int` | Signed checked 64-bit integer |
| `Float` | 64-bit floating-point number |
| `Bool` | `true` or `false` |
| `Char` | One byte |
| `String` | Immutable owned UTF-8 string |
| `Unit` | No useful result; used as a function return type |
| `Array[T]` | Immutable owned collection |
| `Slice[T]` | Immutable retained view into an Array |
| `Option[T]` | `Some(T)` or `None` |
| `Result[T, E]` | `Ok(T)` or `Err(E)` |
| User struct | Immutable named product type |
| User enum | Tagged alternatives with optional payloads |

Types can nest:

```rocket
let rows: Array[Array[String]] = [["name", "score"], ["Ada", "10"]]
let result: Result[Option[Int], String] = Ok(Some(42))
```

Rocket performs no implicit numeric conversions. `Int` and `Float` operands
cannot be mixed without a future explicit conversion API.

## 5. Functions

Parameters and the return type are always explicit.

```rocket
fn add(left: Int, right: Int) -> Int:
    return left + right

fn announce(message: String) -> Unit:
    print(message)
    return
```

A `Unit` function may use bare `return` or fall off the end. Every non-`Unit`
function must return a value on every path.

Calls are positional. Rocket 1.0 has no named, default, variadic, lambda, or
closure syntax.

```rocket
let answer = add(20, 22)
announce("done")
```

### Generic functions

Declare type parameters in brackets. Calls infer them from value arguments.

```rocket
fn identity[T](value: T) -> T:
    return value

let number = identity(42)
let word = identity("Rocket")
```

## 6. Bindings and assignment

`let` creates an immutable binding. `var` permits replacement of the whole
value. The initializer normally infers the type; an annotation may supply it.

```rocket
let name = "Ada"
let score: Int = 10
var total = 0
total = total + score
```

The new value assigned to a `var` must have exactly the original type. There is
no `+=`, `-=`, `++`, or `--` syntax.

Struct fields and Array elements are immutable. This is invalid:

```text
values[0] = 10
pair.first = 10
```

Assign a new whole value instead.

## 7. Operators and precedence

From lowest precedence to highest:

| Level | Operators | Operand/result rules |
| --- | --- | --- |
| 1 | `or` | `Bool`, short-circuiting |
| 2 | `and` | `Bool`, short-circuiting |
| 3 | `==`, `!=` | Same supported type; result `Bool` |
| 4 | `<`, `<=`, `>`, `>=` | Same numeric type; result `Bool` |
| 5 | `+`, `-` | Same numeric type |
| 6 | `*`, `/` | Same numeric type |
| 7 | unary `not`, unary `-` | `Bool` or numeric respectively |
| 8 | call, index, slice, field, postfix `?` | Postfix operations |

Parentheses override precedence.

```rocket
let arithmetic = (2 + 3) * 4
let valid = score >= 0 and score <= 100
let opposite = not valid
let negative = -42
```

Checked `Int` addition, subtraction, multiplication, negation, and division
terminate with a runtime error on overflow. Integer division by zero also
terminates. Rocket 1.0 has no modulo, exponent, bitwise, shift, or ternary
operator.

`String`, scalar numbers, booleans, characters, and `Unit` support the equality
defined by the specification. Arrays, slices, structs, and enums do not have
implicit aggregate equality.

## 8. Conditional control flow

Conditions must be `Bool`.

```rocket
if score >= 50:
    print("pass")
else:
    print("fail")
```

Rocket 1.0 has no separate `else if` token. Nest another `if` inside `else`:

```rocket
if score >= 90:
    print("A")
else:
    if score >= 80:
        print("B")
    else:
        print("C")
```

## 9. Loops

### While loop

```rocket
var count = 0
while count < 3:
    print(count)
    count = count + 1
```

### Exclusive integer range

`start..end` in a `for` loop excludes `end`. Bounds are evaluated once,
left-to-right. The loop variable is an immutable `Int`.

```rocket
for index in 0..3:
    print(index)  # 0, 1, 2
```

### Loop control

```rocket
for index in 0..10:
    if index == 2:
        continue
    if index == 8:
        break
    print(index)
```

`break` and `continue` are valid only inside `while` or `for` bodies.

## 10. Arrays, indexing, and slices

Non-empty Array literals infer their element type:

```rocket
let scores = [10, 20, 30, 40]       # Array[Int]
let names = ["Ada", "Grace"]       # Array[String]
```

An empty Array needs an expected type:

```rocket
let empty: Array[Int] = []
```

Read an element with a zero-based index:

```rocket
print(scores[1])  # 20
```

Create a retained Slice with an exclusive end:

```rocket
let middle = scores[1..3]  # Slice[Int] containing 20, 30
print(middle[0])
```

Index and slice bounds are checked at runtime. Arrays and slices may contain
scalars, strings, structs, enums, and other supported managed values. Element
mutation and general grow/shrink methods are not part of 1.0; use
`collections.concat`, `reverse`, and slices to construct new collections.

## 11. Structs

Struct fields are typed and immutable. Construction is positional; access is by
field name.

```rocket
struct Person:
    name: String
    age: Int

fn main() -> Int:
    let person = Person("Ada", 36)
    print(person.name)
    return 0
```

### Generic structs

```rocket
struct Pair[T]:
    first: T
    second: T

let pair = Pair(10, 20)
let pairs: Array[Pair[Int]] = [pair]
```

## 12. Enums and pattern matching

An enum variant may have no payload or one or more positional payloads.

```rocket
enum Message:
    Number(Int)
    Text(String)
    Quit
```

Construct variants with call syntax, including a zero-payload variant:

```rocket
let first = Number(42)
let second = Text("hello")
let third = Quit()
```

Match every variant and bind payload values immutably:

```rocket
match message:
    case Number(value):
        print(value)
    case Text(text):
        print(text)
    case Quit:
        print("quit")
```

A match must be exhaustive. Its final case may instead be `_`:

```rocket
match message:
    case Quit:
        print("quit")
    case _:
        print("other")
```

Duplicate cases are errors. `_` cannot bind payloads. `match` is a statement,
not a value-producing expression.

## 13. Option, Result, and `?`

`Option[T]` replaces nullable values:

```rocket
fn find(enabled: Bool) -> Option[Int]:
    if enabled:
        return Some(42)
    return None()

match find(true):
    case Some(value):
        print(value)
    case None:
        print("missing")
```

`Result[T, E]` represents success or recoverable failure:

```rocket
fn parse(value: String) -> Result[Int, String]:
    return string.parse_int(value)

match parse("42"):
    case Ok(value):
        print(value)
    case Err(error):
        print(error)
```

Postfix `?` unwraps a success or returns the matching failure from the current
function:

```rocket
fn increment(text: String) -> Result[Int, String]:
    let value = string.parse_int(text)?
    return Ok(value + 1)
```

`?` on `Option[T]` requires an `Option[...]` return type. `?` on
`Result[T, E]` requires a `Result[..., E]` return type with the same error type.
Rocket has no exceptions, `throw`, `try`, or `catch`.

## 14. Modules, imports, and visibility

Each `.rocket` file is a module. Dots in an import map to directories relative
to the package root.

```text
my_package/
  rocket.toml
  src/
    main.rocket
    math.rocket
```

`src/math.rocket`:

```rocket
pub struct Pair[T]:
    first: T
    second: T

pub enum Signal:
    Value(Int)
    Empty

pub fn doubled(value: Int) -> Int:
    return value * 2

fn hidden() -> Int:
    return 99
```

`src/main.rocket`:

```rocket
import src.math

fn main() -> Int:
    let pair: math.Pair[Int] = math.Pair(3, 4)
    print(math.doubled(pair.first))
    let signal = math.Value(pair.second)
    match signal:
        case math.Value(value):
            print(value)
        case math.Empty:
            print(0)
    return 0
```

The final import component becomes the local alias (`math` above). There is no
custom `as` alias syntax. Cross-file functions, structs, enums, and variants
must be marked `pub`. Imports are recursive; missing modules, alias collisions,
private access, and cycles are compile-time errors.

Only `import`, `fn`, `struct`, and `enum` declarations, optionally prefixed by
`pub`, are allowed at top level.

## 15. Standard modules

Import a standard module, then call it through its final component:

```rocket
import std.string
import std.collections

fn main() -> Int:
    let words = string.split("one,two", ",")
    print(collections.length(words))
    return 0
```

### `std.string`

```text
string.byte_length(String) -> Int
string.concat(String, String) -> String
string.contains(String, String) -> Bool
string.starts_with(String, String) -> Bool
string.ends_with(String, String) -> Bool
string.trim(String) -> String
string.split(String, String) -> Array[String]
string.byte_at(String, Int) -> Char
string.byte_value_at(String, Int) -> Int
string.slice(String, Int, Int) -> String
string.parse_int(String) -> Result[Int, String]
string.from_int(Int) -> String
string.builder() -> std.string.Builder
string.builder_append(std.string.Builder, String) -> Unit
string.builder_finish(std.string.Builder) -> String
```

String indices and slices are UTF-8 byte offsets. `Builder` is the one explicit
mutable text-construction object; `builder_finish` returns an immutable String.

### `std.collections`

```text
collections.length[T](Array[T]) -> Int
collections.slice_length[T](Slice[T]) -> Int
collections.reverse[T](Array[T]) -> Array[T]
collections.concat[T](Array[T], Array[T]) -> Array[T]
collections.join(Array[String], String) -> String
```

### `std.file`

```text
file.read_text(String) -> Result[String, String]
file.write_text(String, String) -> Result[Bool, String]
file.append_text(String, String) -> Result[Bool, String]
file.exists(String) -> Bool
file.remove(String) -> Result[Bool, String]
file.list(String) -> Result[Array[String], String]
file.create_directory(String) -> Result[Bool, String]
```

### `std.path`

```text
path.join(String, String) -> String
path.basename(String) -> String
path.extension(String) -> String
path.normalize(String) -> String
```

### `std.json`

```text
json.parse(String) -> Result[std.json.Json, String]
json.stringify(std.json.Json) -> String
```

The built-in JSON declarations are equivalent to:

```rocket
enum Json:
    Null
    Boolean(Bool)
    Integer(Int)
    Decimal(Float)
    Text(String)
    List(Array[Json])
    Object(Array[JsonField])

struct JsonField:
    key: String
    value: Json
```

### `std.csv`

```text
csv.parse(String) -> Result[Array[Array[String]], String]
csv.encode(Array[Array[String]]) -> String
```

### `std.random`

```text
random.seed(Int) -> Unit
random.int(Int, Int) -> Int
random.float() -> Float
```

`random.int(minimum, maximum)` uses a half-open range. Rocket 1.0 randomness is
deterministic and is not cryptographically secure.

### `std.process`

```text
process.run(String, Array[String]) -> Result[Int, String]
process.arguments() -> Array[String]
process.executable_path() -> Result[String, String]
process.environment(String) -> Option[String]
process.working_directory() -> Result[String, String]
```

`process.run` starts a program directly without a command shell.

### `std.time`

```text
time.unix_milliseconds() -> Int
time.monotonic_milliseconds() -> Int
time.sleep_milliseconds(Int) -> Unit
```

## 16. Built-in `print`

`print(value)` writes one scalar value plus a newline and returns `Unit`.

```rocket
print(42)
print(1.5)
print(true)
print('R')
print("Rocket")
```

In Rocket 1.0, booleans print as `1` and `0`. Arrays and user aggregates do not
have automatic display formatting; print their fields/elements explicitly or
encode them through a suitable standard API.

## 17. Package dictionary

`rocket.toml`:

```toml
[package]
name = "hello"
version = "1.0.0"
entry = "src/main.rocket"

[test]
directory = "tests"
```

Stable layout:

```text
hello/
  rocket.toml
  src/
    main.rocket
  tests/
    smoke_test.rocket
```

Each test file is an ordinary independent Rocket program with
`fn main() -> Int`; exit `0` passes. Generated files go in `.rocketc/`.

## 18. Compiler command dictionary

```powershell
rocketc --version
rocketc --help
rocketc new <directory>
rocketc check <file-or-package>
rocketc build <file-or-package>
rocketc run <file-or-package>
rocketc run <file-or-package> -- <program-arguments...>
rocketc test <package>
rocketc fmt <file-or-package>
rocketc fmt <file-or-package> --check
rocketc emit-ir <file-or-package> <output.ll>
rocketc emit-asm <file-or-package> <output.s>
```

- `check` parses, resolves imports, and type-checks without producing a program.
- `build` creates `.rocketc/main.exe`.
- `run` builds and runs the program.
- `test` discovers `.rocket` tests recursively in lexical path order.
- `fmt` writes canonical LF/four-space formatting; `--check` changes nothing.
- `emit-ir` and `emit-asm` expose native compiler output.

## 19. VS Code usage

The repository extension provides syntax highlighting, indentation/bracket
behavior, snippets, tasks, and clickable `Rdddd` diagnostics. Install or copy
`editors/vscode` as `rocket-lang.rocket-language-1.0.0`, reload VS Code, and use:

```text
Terminal -> Run Task -> Rocket: Check package
Terminal -> Run Task -> Rocket: Run package
Terminal -> Run Task -> Rocket: Test package
Terminal -> Run Task -> Rocket: Format check
```

The extension has no language server. Rocket 1.0 therefore does not yet provide
semantic completion, hover types, go-to-definition, rename, or cross-file
IntelliSense.

## 20. Features not present in Rocket 1.0

Do not assume syntax for these features:

- Built-in maps/dictionaries, sets, tuples, linked lists, or a general collection framework
- Classes, inheritance, interfaces/traits, methods, or field mutation
- Exceptions, `null`, nullable references, or optional chaining
- Lambdas, closures, coroutines, generators, async/await, or threads
- Macros, reflection, operator overloading, or function overloading
- Package registry/dependency manager or third-party package imports
- Array element mutation, append/pop methods, or implicit collection equality
- String interpolation, multiline strings, regex literals, or full Unicode
  scalar/grapheme iteration
- `switch`, ternary expressions, `else if`, C-style loops, `do/while`, or
  inclusive range syntax
- Implicit numeric conversion, modulo, exponentiation, bitwise operators,
  compound assignment, increment, or decrement
- Linux, macOS, WebAssembly, JIT, or cross-compilation targets

Rocket 1.0 is a complete implementation of its frozen scope, not yet a
feature-for-feature replacement for mature languages such as C++, Python,
Java, Rust, or C#.

## 21. Complete example

```rocket
import std.collections
import std.string

struct Pair[T]:
    first: T
    second: T

enum Message:
    Number(Int)
    Text(String)

fn parse_and_increment(text: String) -> Result[Int, String]:
    let value = string.parse_int(text)?
    return Ok(value + 1)

fn main() -> Int:
    let pair = Pair(10, 20)
    let values = [pair.first, pair.second, 30]
    let middle = values[1..3]
    print(collections.slice_length(middle))

    let result = parse_and_increment("41")
    match result:
        case Ok(value):
            print(value)
        case Err(error):
            print(error)

    let message = Text("done")
    match message:
        case Number(value):
            print(value)
        case Text(text):
            print(text)
    return 0
```

Canonical specifications remain in `SPEC.md`, `STDLIB.md`, `TOOLING.md`, and
`DIAGNOSTICS.md`. The complete example is also available as
`examples/language_tour.rocket`.
