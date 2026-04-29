-- single line comments
--- documentation comment, attaches to the next declaration

-- ==========================================================================
-- primitives
-- ==========================================================================

int x = 1;
float y = 1.5;
bool flag = true;
string s = "hello";
-- null and void are types too
-- void is the implied return type of functions that return nothing
-- null is both a type and its only value

-- ==========================================================================
-- bindings
-- ==========================================================================

-- explicit type leads, always immutable
int x = 1;
float y = 1.5;
string s = "hello";

-- inferred, immutable
let x = 1;

-- inferred, mutable
mut x = 1;
mut x += 1;
mut x -= 1;
mut x \*= 2;
mut x /= 2;
mut x %= 3;

-- casting
let sum = x as float + y;

-- optional type
?int maybe = null;
let maybe = null;

-- ==========================================================================
-- functions
-- ==========================================================================

-- forward declaration
add(int a, int b) -> int;

-- definition (last expression is implicit return)
add(int a, int b) -> int {
a + b
}

-- void return, implied
main() {
}

-- multiple return via tuples
divrem(int a, int b) -> (int, int) {
(a / b, a % b)
}

-- explicit early return
clamp(int x, int lo, int hi) -> int {
if x < lo { return lo; }
if x > hi { return hi; }
x
}

-- ==========================================================================
-- arrays and slices
-- ==========================================================================

-- fixed size, explicit type
[int; 4] array = [1, 2, 3, 4];

-- inferred
let list = ["hello", "from", "verdigris"];

-- slice — a view into an array or vector, does not own its memory
&[int] slice = &array;
let slice = &array;

-- slice range
&[int] part = &array[0..2];

-- ==========================================================================
-- vectors
-- ==========================================================================

mut vector v = [1, 1, 2, 3];
let w = vector[4, 5, 6];
v.push(5);
let first = v[0];
let last = v[v.len - 1];

-- ==========================================================================
-- tuples
-- ==========================================================================

(int, string, bool) t = (1, "hello", true);
let (num, word, flag) = t;

-- ==========================================================================
-- strings
-- ==========================================================================

let name = "verdigris";

-- interpolation — {} in format strings, evaluated at call site
print("hello, {}!", name);

-- format specifiers
print("{:d}", 42);
print("{:x}", 255);
print("{:.2f}", 3.14159);
print("{!}", v); -- debug representation

-- ==========================================================================
-- structs
-- ==========================================================================

struct Point {
int x, int y

    -- method signatures only inside the struct body
    distance(self, Point other) -> float;
    origin(mut self) -> Point;
    translate(mut self, int dx, int dy);

}

-- struct instantiation
Point p = { x: 0, y: 0 };

-- method implementation lives outside the struct
Point::distance(self, Point other) -> float {
pow(
pow(other.x - self.x, 2) +
pow(other.y - self.y, 2),
0.5)
}

Point::origin(mut self) -> Point {
{ x: 0, y: 0 }
}

Point::translate(mut self, int dx, int dy) {
self.x += dx;
self.y += dy;
}

-- usage
mut p = Point{ x: 0, y: 0 };
let area = p.distance({ x: 3, y: 4 });
p.translate(1, 2);

-- ==========================================================================
-- enums (algebraic data types)
-- ==========================================================================

enum Direction {
Left, Right, Up, Down
}

-- enum variants with data
enum Shape {
Circle(float),
Rectangle(float, float),
Point,
}

-- generic enum
enum Option[T] {
Some(T),
None,
}

enum Result[T, E] {
Ok(T),
Err(E),
}

-- recursive enum
enum List[T] {
Cons(T, List[T]),
Nil,
}

Direction d = Up;
let d = Direction::Up;

-- ==========================================================================
-- pattern matching
-- ==========================================================================

fib(int n) -> int {
match n {
0 => 1,
1 => 1,
\_ => fib(n - 1) + fib(n - 2),
}
}

-- matching on enum variants with data
area(Shape s) -> float {
match s {
Circle(r) => 3.14159 _ r _ r,
Rectangle(w, h) => w \* h,
Point => 0.0,
}
}

-- pattern guards
classify(int n) -> string {
match n {
n if n < 0 => "negative",
n if n > 0 => "positive",
0 => "zero",
}
}

-- destructuring
let (x, y) = (1, 2);

match list {
Cons(head, tail) => process(head, tail),
Nil => print("empty"),
}

-- ==========================================================================
-- error handling
-- ==========================================================================

-- functions that can fail return union types
open(string path) -> File | IOError;
find_user(int id) -> User | null;

-- ! propagates the error to the caller
file = open("data.txt")!;
user = find_user(42)!;

-- catch recovers inline
file = open("data.txt") catch |e| default_file;

-- catch with a block
file = open("data.txt") catch |e| {
log("failed: {}", e);
backup = open("backup.txt")!;
return backup;
};

-- explicitly ignore the error
\_ = open("optional.txt");

-- match on the result
match open("data.txt") {
File f => process(f),
IOError e => {
log("failed: {}", e);
recover();
},
}

-- ==========================================================================
-- pipelines
-- ==========================================================================

-- >> composes left to right, passing the result of each step as the
-- first argument to the next. equivalent to nested function calls
-- but reads in the order things actually happen.

let result = data >> filter(.active) >> map(.name) >> sort >> collect;

-- with error propagation
let ast = path >> open! >> read_all! >> parse!;

-- with recovery
let content = path >> open! >> read_all catch |e| "" >> parse!;

-- ==========================================================================
-- interfaces
-- ==========================================================================

-- structural contracts — no explicit declaration needed on the type
interface Printable {
format(self) -> string;
}

interface Eq {
eq(self, Eq other) -> bool;
}

interface Hashable {
hash(self) -> int;
}

-- interface composition
interface HashableEq {
Hashable;
Eq;
}

-- implementing an interface — just implement the methods
Point::format(self) -> string {
"({}, {})".fmt(self.x, self.y)
}

Point::eq(self, Point other) -> bool {
self.x == other.x && self.y == other.y
}

-- ==========================================================================
-- operator overloading via interfaces
-- ==========================================================================

interface Add[Rhs] {
add(self, Rhs rhs) -> Self;
}

struct Vec3 { float x, float y, float z }

Vec3::add(self, Vec3 other) -> Vec3 {
Vec3{ x: self.x + other.x, y: self.y + other.y, z: self.z + other.z }
}

-- now + works on Vec3
let v3 = v1 + v2;

-- ==========================================================================
-- allocators
-- ==========================================================================

-- arena — fast bump allocator, bulk reset, no individual frees
Arena arena = Arena(4096);

-- heap — general purpose
Heap heap = Heap();

-- allocation via pipeline
let data = arena >> alloc(User, 10);
let buf = arena >> alloc_bytes(1024);

-- scoped arena with defer
{
temp = Arena(4096);
defer temp.reset();

    let scratch = temp >> alloc(1024);
    process(scratch);
    -- temp.reset() runs automatically at scope exit

}

-- ==========================================================================
-- control flow
-- ==========================================================================

for i in array { }
for i in 0..10 { }
for i in 0..=10 { } -- inclusive range

if x == 1 {
} else if x == 2 {
} else {
}

-- if as expression
bool result = if true { true } else { false };

while x == 1 { }

loop {
if condition { break; }
if other { continue; }
}

-- ==========================================================================
-- attributes
-- ==========================================================================

@inline
small_helper(int x) -> int {
x \* 2
}

@deprecated("use new_add instead")
old_add(int a, int b) -> int {
a + b
}

-- ffi
@extern("C")
c_sqrt(float x) -> float;

-- ==========================================================================
-- modules (planned)
-- ==========================================================================

import "std/vec";
import "std/io" as io;

-- public export
pub add(int a, int b) -> int {
a + b
}

-- ==========================================================================
-- logical operators
-- ==========================================================================

bool a = true && false;
bool b = true || false;
bool c = !true;

-- ==========================================================================
-- printing
-- ==========================================================================

print("{} {}", a, b);
print("{!}", v);
print("{:.2f}", 3.14159);
