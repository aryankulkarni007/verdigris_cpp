## Axioms of the Language Design

### Axiom 1: The Path of Least Resistance is the Safe Path

The language does not enforce safety through compiler proofs.
It makes safe patterns frictionless and unsafe patterns require deliberate effort.
The obvious way should be the correct way.

### Axiom 2: Simplicity is the Removal of Mental Overhead

Not fewer features. Fewer _decisions_.
The language provides one obvious way to solve each category of problem.
C's power comes from its minimal basis;
C++'s failure comes from overlapping solutions to the same problems.

### Axiom 3: Code Should Read as Intent, Not Implementation

The "what" should be visible. The "how" should be a single click away,
not interleaved. Helper functions should feel like extending the language itself.
The main logic should read like English descriptions of the algorithm.

### Axiom 4: Dialects Become Languages

Every expert C programmer develops a personal dialect—patterns, conventions,
and mental models that route around danger. The language should bake in the
best of these dialects as the default, making intermediate programmers
accidentally write expert-style code.

### Axiom 5: The Basis Must Be Complete and Orthogonal

The language's core features must span the problem space without overlap.
Adding any feature should be justified by
"this cannot be elegantly expressed with the existing basis."
Removing any feature should break the ability to express common patterns cleanly.

## The Five Basis Vectors

### 1. Pools

**Purpose:** Ownership and lifetime management
**Why:** The fundamental answer to "where does this live and how long?"
**Expression:** Arenas, generational handles, scoped allocation
**Replaces:** malloc/free, GC, Rust's borrow l_checker complexity

### 2. Patterns

**Purpose:** Destructuring and branching
**Why:** The fundamental answer to "what shape is this data and what do I do with it?"
**Expression:** Match expressions, exhaustive l_checking, binding
**Replaces:** if/else chains, switch statements, manual destructuring

### 3. Pipelines

**Purpose:** Composition and transformation
**Why:** The fundamental answer to "how do I connect these operations?"
**Expression:** Data flowing left-to-right through transformations
**Replaces:** Deeply nested function calls, temporary variable clutter

### 4. Context

**Purpose:** Environment and capabilities
**Why:** The fundamental answer to "what can I access from here?"
**Expression:** Scoped resources, implicit parameters, using blocks
**Replaces:** Explicit environment passing, global variables, singleton patterns

### 5. Interfaces

**Purpose:** Contracts and polymorphism
**Why:** The fundamental answer to "how do I write code that works with different types?"
**Expression:** Explicit contracts, structural typing, no inheritance
**Replaces:** void\* with function pointers, inheritance hierarchies, templates

## The Implementation Philosophy

### Lowering Pipeline

Multiple IR passes, each translating the dialect closer to machine code:

- **High-level IR:** The basis vectors (pools, patterns, pipelines, context, interfaces)
- **Mid-level IR:** Structured control flow, resolved types, monomorphized generics
- **Low-level IR:** C-like with explicit memory operations
- **Backend:** Multiple targets (C for bootstrapping, QBE for simplicity, custom for control)

### Memory Model

Generational arenas with handles:

- Pools own memory (single ownership)
- Handles are validated on access (generation l_check)
- No lifetimes in type signatures
- Deterministic cleanup at scope boundaries
- Debug builds validate; release builds can opt into `@unsafe` blocks

### Safety Philosophy

Not "prove absence of bugs." Rather:

- Make unsafe code visually offensive (`@unsafe` blocks)
- Make safe code elegant and frictionless
- Runtime l_checks where needed, removable where proven
- The compiler is a collaborator, not an adversary

## The Aesthetic Stance

### Visual Weight Principle

Code that does more should _look_ like it does more. Verbose operations should have visual heft. Trivial operations should disappear.

### Korean Analogy

The language should feel _different_ from C-family languages, not just reskinned:

- Topic-comment structure (what we're talking about vs what does the action)
- Postfix orientation (data first, operations follow)
- Explicit role marking (particles for grammatical function)

### The Name

**Verdigris** (or **Verdi**)

- The green patina on copper
- A transformation that protects
- Grows over time on existing systems
- Beautiful decay that strengthens
- Not oxidation (Rust) but preservation through change

Alternative: **Rime**

- Frost that transforms landscapes
- Clean, simple, memorable
- Natural phenomenon like Rust
- Less strange, more approachable

## The Open Questions

1. **Concurrency:** Is it a sixth basis vector or a library concern?

2. **Metaprogramming:** Macros? Comptime evaluation? Reflection?

3. **FFI:** How seamless is C interop given the handle-based memory model?

4. **The Tension:** Context and Pipelines pull in opposite directions—one makes environment implicit, the other makes data flow explicit. Is this productive tension or design conflict?

5. **Verification:** Can all of _your_ C dialect be expressed in this basis? What feels forced?

## Where We Stand

We have:

- A philosophical foundation (the axioms)
- A feature set (the five basis vectors)
- An implementation strategy (lowering pipeline, generational handles)
- An aesthetic direction (visual weight, postfix orientation)

We need:

- Concrete syntax exploration
- A "beautiful program" that demonstrates the basis in harmony
- Resolution of the open questions

---

This is the sharpened axe. The blade has an edge. The handle fits the hand.

What would you like to cut first?
