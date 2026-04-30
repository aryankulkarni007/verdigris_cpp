# Verdigris Language Specification

## Design Axioms

1. **Pools** - Allocators as types with generational handles for memory safety without garbage collection
2. **Patterns** - Exhaustive pattern matching over algebraic data types
3. **Pipelines** - Left-to-right composition with `>>` operator
4. **ADTs** - Algebraic data types (structs as products, enums as sums)
5. **Interfaces** - Structural contracts without inheritance
6. **Generics** - Type parameters with interface constraints

## Syntax

### Comments

```
-- single line comment
-*
 * block comment
-*
--- documentation comment (attaches to following declaration)
```

### Primitive Types

```
int       -- signed 64-bit integer
float     -- 64-bit floating point
bool      -- true or false
char      -- UTF-8 character (32-bit)
string    -- UTF-8 string
void      -- unit type
null      -- null type (only value is null)
```

### Variables

```
-- Explicit type, immutable
int x = 1;
float y = 1.5;
string s = "hello";

-- Type inference, immutable
let x = 1;

-- Mutable
mut x = 1;
mut x += 1;
```

### Functions

```
-- Forward declaration
add(int a, int b) -> int;

-- Definition (last expression is return value)
add(int a, int b) -> int {
    a + b
}

-- Void return (implied)
main() {
    print("hello");
}

-- Multiple returns via tuples
divrem(int a, int b) -> (int, int) {
    (a / b, a % b)
}

-- Generic functions
identity[T](value: T) -> T {
    value
}
```

### Structs

```
struct Point {
    x: int,
    y: int,
}

-- Instantiation
Point p = { x: 0, y: 0 };

-- With methods
struct Circle {
    center: Point,
    radius: float,
}

Circle::area(self) -> float {
    3.14159 * self.radius * self.radius
}

Circle::translate(mut self, dx: float, dy: float) {
    self.center.x = self.center.x + dx;
    self.center.y = self.center.y + dy;
}

-- Usage
mut c = Circle{ center: { x: 0, y: 0 }, radius: 5.0 };
area = c.area();
c.translate(1.0, 2.0);
```

### Enums (Algebraic Data Types)

```
enum Option[T] {
    Some(T),
    None,
}

enum Result[T, E] {
    Ok(T),
    Err(E),
}

enum Shape {
    Circle(radius: float),
    Rectangle(width: float, height: float),
    Point,
}

-- Recursive types
enum List[T] {
    Cons(T, List[T]),
    null,
}
```

### Pattern Matching

```
-- Match expression
match value {
    pattern1 => expression1,
    pattern2 => expression2,
    _ => default,
}

-- Examples
area(shape: Shape) -> float {
    match shape {
        Circle(r) => 3.14159 * r * r,
        Rectangle(w, h) => w * h,
        Point => 0.0,
    }
}

-- Destructuring
let (x, y) = point_to_tuple(p);

match list {
    Cons(head, tail) => process(head, tail),
    null => print("empty"),
}

-- Pattern guards
match value {
    n if n < 0 => "negative",
    n if n > 0 => "positive",
    0 => "zero",
}
```

### Error Handling

```
-- Functions return union types for fallibility
open(path: string) -> File | IOError;
find_user(id: int) -> User | null;

-- Explicit propagation with !
file = open("data.txt")!;
user = find_user(42)!;

-- Inline recovery with catch (single expression)
file = open("data.txt") catch |e| default_file;
user = find_user(42) catch guest_user;

-- Inline recovery with catch (block)
file = open("data.txt") catch |e| {
    log("failed to open '{}': {}", path, e);
    backup = open("backup.txt")!;
    return backup;
};

-- Pipeline with propagation
ast = path >> open! >> read_all! >> parse!;

-- Pipeline with recovery
content = path
    >> open!
    >> read_all catch |e| ""
    >> parse!;

-- Complex handling with match
match open("data.txt") {
    File f => process(f),
    IOError e => {
        -- multi-line recovery logic
        log("failed: {}", e);
        recover();
    },
}

-- Explicit ignore
_ = open("optional.txt");
```

### Pipelines

```
-- Left-to-right composition with >>
result = data
    >> filter(.active)
    >> map(.name)
    >> sort
    >> collect;

-- With error propagation
ast = path
    >> open!
    >> read_all!
    >> parse!;

-- Equivalent to
file = open(path)!;
content = read_all(file)!;
ast = parse(content)!;
```

### Interfaces

```
-- Interface definition
interface Printable {
    format(self) -> string;
}

interface Hashable {
    hash(self) -> uint;
}

interface Eq {
    eq(self, other: Self) -> bool;
}

-- Implementation (structural - no declaration needed)
Point::format(self) -> string {
    "({}, {})".fmt(self.x, self.y)
}

Point::eq(self, other: Point) -> bool {
    self.x == other.x && self.y == other.y
}

-- Interface composition
interface HashableEq {
    Hashable;
    Eq;
}

-- Generic constraints
get_or_insert[K: Hashable + Eq, V](
    map: Map[K, V],
    key: K,
    default: V
) -> V {
    -- ...
}

-- Builtin implementations
int::format(self) -> string { ... }
int::hash(self) -> uint { ... }
int::eq(self, other: int) -> bool { self == other }

string::format(self) -> string { self }
string::hash(self) -> uint { ... }
string::eq(self, other: string) -> bool { ... }
```

### Operator Overloading

```
-- Arithmetic operators via interfaces
interface Add[Rhs = Self] {
    add(self, rhs: Rhs) -> Self;
}

interface Sub[Rhs = Self] {
    sub(self, rhs: Rhs) -> Self;
}

interface Mul[Rhs = Self] {
    mul(self, rhs: Rhs) -> Self;
}

interface Div[Rhs = Self] {
    div(self, rhs: Rhs) -> Self;
}

-- Implement for custom types
Vector3::add(self, other: Vector3) -> Vector3 {
    Vector3{
        x: self.x + other.x,
        y: self.y + other.y,
        z: self.z + other.z,
    }
}

-- Now + works
v3 = v1 + v2;

-- Comparison operators
interface PartialEq[Rhs = Self] {
    eq(self, rhs: Rhs) -> bool;
    ne(self, rhs: Rhs) -> bool;
}

interface PartialOrd[Rhs = Self] {
    lt(self, rhs: Rhs) -> bool;
    le(self, rhs: Rhs) -> bool;
    gt(self, rhs: Rhs) -> bool;
    ge(self, rhs: Rhs) -> bool;
}
```

### Arrays and Slices

```
-- Fixed-size array
int[4] fixed = [1, 2, 3, 4];

-- Slice (view into array)
int[] slice = fixed[0..2];  -- [1, 2]

-- Inferred array
let inferred = [1, 2, 3, 4, 5];
```

### Vectors (Standard Library)

```
-- Dynamic array
mut Vec[int] v = Vec.new();
v.push(1);
v.push(2);
v.push(3);

-- Literal construction
let v = Vec[1, 2, 3, 4, 5];

-- Access
first = v[0];
last = v[v.len - 1];

-- Iteration
for item in v {
    print(item);
}
```

### Strings (Standard Library)

```
-- Creation
let s = "hello";
let multi =
    "multiline
    string";

-- Interpolation
let name = "World";
let greeting = "Hello, {name}!";

-- Format specifiers
print("{:d}", 42);        -- decimal
print("{:x}", 255);       -- hex
print("{:.2f}", 3.14159); -- float precision
print("{!}", value);      -- debug representation
```

### Allocators (Pools)

```
-- Allocator types
Arena arena = Arena(4096);
Heap heap = Heap();
Stack stack = Stack(1024);

-- Allocation via piping
data = arena >> alloc(User, 10);
buffer = arena >> alloc_bytes(1024);
handle = arena >> push(user);

-- Manual allocation
User* users = arena.alloc(User, 10);
arena.free(handle);
arena.reset();

-- Scoped arena (library pattern)
{
    temp = Arena(4096);
    defer temp.reset();

    data = temp >> alloc(1024);
    process(data);
    -- auto-reset at end
}
```

### Control Flow

```
-- If expression
if x > 0 {
    print("positive");
} else if x < 0 {
    print("negative");
} else {
    print("zero");
}

-- If as expression
let result = if true { "yes" } else { "no" };

-- While loop
while x < 10 {
    x = x + 1;
}

-- Infinite loop
loop {
    if condition {
        break;
    }
}

-- For loop
for i in 0..10 {
    print(i);
}

for item in array {
    print(item);
}

-- Break and continue
for i in 0..10 {
    if i == 3 {
        continue;
    }
    if i == 7 {
        break;
    }
    print(i);
}
```

### Casting

```
-- Type conversion
let x: float = 3.14;
let y = x as int;  -- 3

-- Interface casting (runtime l_checked)
let printable = value as Printable;
match printable {
    Some(p) => print(p.format()),
    None => print("not printable"),
}
```

### Modules (Planned)

```
-- Import
import "std/vec";
import "std/io" as io;

-- Export (public)
pub Point = struct { ... };

pub add(int a, int b) -> int { ... }
```

### Attributes (Planned)

```
-- Compiler hints
@inline
small_helper() { ... }

@deprecated("use new_function instead")
old_function() { ... }

-- FFI
@extern("C")
foreign_function() -> int;
```

## Memory Model

### Generational Handles

The language provides memory safety through pools with generational handles:

```
-- Pool owns memory
users: Pool[User] = Pool.new();

-- Allocation returns handle
alice: Handle[User] = users.alloc(User{name: "Alice"});

-- Access validates handle
match users.get(alice) {
    Some(user) => print(user.name),
    None => print("invalid handle"),
}

-- Explicit free
users.free(alice);

-- Bulk reset
users.reset();  -- all handles invalidated
```

### Ownership Rules

1. Values have a single owner (the pool they're allocated in)
2. Handles are copyable integer tokens
3. Handle access validates generation (prevents use-after-free)
4. Pools can be arena-allocated and freed together
5. No lifetimes in type signatures

## Standard Library Outline

```
std/
  arena.vg       -- Arena allocator
  heap.vg        -- Heap allocator
  vec.vg         -- Dynamic array
  string.vg      -- UTF-8 string operations
  map.vg         -- Hash map
  set.vg         -- Hash set
  io.vg          -- File and console I/O
  fmt.vg         -- Formatting and printing
  math.vg        -- Mathematical functions
  os.vg          -- Operating system interface
  thread.vg      -- Concurrency primitives
  channel.vg     -- CSP-style channels
  json.vg        -- JSON parsing/serialization
  test.vg        -- Testing framework
```

## Philosophy

verdigris is "c but expressive." It provides:

- Zero-cost abstractions through generics and interfaces
- Memory safety without garbage collection via pools and handles
- Functional expressiveness through ADTs and pattern matching
- Structural polymorphism without inheritance
- Explicit error handling with union types
- Pipeline composition for data transformation

The language does not enforce safety through complex static analysis.
Instead, it makes safe patterns frictionless and unsafe patterns require deliberate effort.
The path of least resistance is the safe path.

This document is a living specification. Syntax and features may evolve during implementation.
