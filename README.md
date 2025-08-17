# Mini Compiler (Parser / Front-End Prototype)

A small C implementation of a toy programming language. It currently performs:

- File loading → Lexing → Parsing → (AST dump / token dump optional)
- Error reporting with line/column and minimal recovery (stop-on-first for now)
- Internal test-suite executed before compiling a user file

## Language Snapshot
Supports:
- Variable declarations (with `const`, arrays, pointer-like postfix stars)
- Primitive types: i32, i64, f32, f64, bool
- Functions (single return type, parameters, recursion)
- Expressions: literals, identifiers, prefix/postfix ++/--, unary ops, binary ops with precedence, assignment / compound assignment chaining
- Control flow: if / else-if / else, while, for (C-style), break, continue, return
- Array subscripts, function calls, nested initializer lists `{ ... }`

Planned / partial: constant expressions for array sizes, semantic analysis, type checking, code generation.

## Project Layout
```
include/    Public headers (lexer, parser, ast, dynamic arrays, hash map)
src/        Implementations
input/      Grammar (lang.bnf) and sample inputs
tokens.def  Token macro list
tests       (implicit via hardcoded suite in main.c)
```

## Build
```
make        # builds executable (default target)
make clean
```
Requires a POSIX environment (tested macOS) and a C compiler supporting C11 or later.

## Running
```
./compiler-new <file> [--tokens] [--ast] [--time] [--show-errors]
```
On startup it runs an internal parser test-suite (hardcoded cases) summarizing pass/fail counts, then processes the given source file.

## Core Components
### Lexer
- Single-pass, hand-written scanner over a NUL-terminated buffer
- Emits tokens into a dynamic array (ending with TOK_EOF)
- Skips comments & whitespace; basic error on unknown tokens

### Parser
- Recursive-descent with a small helper for left-associative binary expression levels.
- Operator precedence achieved by layering functions: logical-or → logical-and → equality → relational → additive → multiplicative → unary → postfix → primary.
- Assignment handled as a special case after parsing a logical-or (right-associative) by re-wrapping the LHS.
- Control-flow nodes allocate structured AST (if / while / for) with branches & optional pieces.
- For-loop supports optional init / condition / post segments similar to C.
- Blocks collect statements in a growable array.

### AST & Memory
- Each node tagged by enum; unions store typed payloads.
- Dynamic arrays (stretchy buffers) for lists (statements, params, args, initializer elements, type dimensions).
- Centralized free routine recursively releases subtrees to avoid leaks; careful error-path cleanup to avoid double-free.

### Error Handling
- ParseError structure with message + position; first fatal error stops parsing.
- Some convenience flags (underline previous token line) for tighter diagnostic anchoring.

### Testing Harness
- Embedded suite in `main.c`; each test writes a temporary file, runs the front-end, checks expected success/failure.
- Compact summary: only unexpected results print source snippet.

## Grammar vs Implementation Notes
- Grammar (`input/lang.bnf`) requires braces for control-flow bodies; implementation currently enforces blocks (single-statement bodies without braces can be enabled as an extension if desired).
- String literals supported though not originally listed in `<Primary>` section.
- Function call treated as expression (no separate `<FunctionCall>` statement rule internally).

## Example
Source: prog.lang
```c
fn add(a: i32, b: i32) -> i32 { return a + b; }
fn main() { x: i32 = 0; for (i: i32 = 0; i < 10; i = i + 1) { x += i; } }
```
Run:
```
./compiler-new prog.lang --ast
```
Outputs an AST tree dump (if enabled) or exits silently on success.

```yaml
=== AST ===
Program:
  Function: add
    ReturnType:
      Base-type: i32
    Parameters:
      Param: a
        Base-type: i32
      Param: b
        Base-type: i32
    Body:
      Block:
        ReturnStatement:
          BinaryOp: +
            Variable: a
            Variable: b
  Function: main
    Parameters: (none)
    Body:
      Block:
        Declaration:
          Variable: x
            Type:
              Base-type: i32
            Initializer:
              Literal: 0
        ForLoop:
          Init:
            Declaration:
              Variable: i
                Type:
                  Base-type: i32
                Initializer:
                  Literal: 0
          Condition:
            BinaryOp: <
              Variable: i
              Literal: 10
          Post:
            Assignment: =
              Variable: i
              BinaryOp: +
                Variable: i
                Literal: 1
```


## Extending
Ideas:
- Semantic pass (type table, scope chain, symbol resolution)
- Constant folding & simple IR generation
- Emission (e.g., C, WASM, or bytecode)
- Proper error recovery (synchronization on semicolons / braces)
- Real test framework
- Hash map utility completion for symbol tables

## Limitations
- No type checking / symbol resolution yet
- Minimal diagnostics & no recovery
- No codegen / optimization
