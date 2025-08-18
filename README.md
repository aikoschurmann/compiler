# Mini Compiler (Parser / Front-End Prototype)

A small C implementation of a toy programming language. It currently performs:

- File loading → Lexing → Parsing → (AST dump / token dump optional)
- Error reporting with line/column and minimal recovery (stop-on-first for now)
- Internal test-suite executed before compiling a user file

## Language Snapshot
Supports:
- Variable declarations (with `const`, arrays, pointer-like postfix stars)
- Primitive types: i32, i64, f32, f64, bool
- Complex type expressions: function types `fn(param_types) -> return_type`, arrays with expression dimensions, pointers, parenthesized grouping
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
- **Enhanced type parsing**: Correctly handles complex type expressions including:
  - Function types: `fn(i32, i32) -> i32`, `fn() -> fn(i32) -> i32`
  - Array types with expression dimensions: `i32[10]`, `i32[x + 1]`
  - Pointer types with proper precedence: `i32*[10]` vs `i32[10]*`
  - Parenthesized grouping: `(i32*)[10]` and `(fn())[]`
  - Complex combinations: `fn(i32*) -> i32[]*`
- Operator precedence achieved by layering functions: logical-or → logical-and → equality → relational → additive → multiplicative → unary → postfix → primary.
- Assignment handled as a special case after parsing a logical-or (right-associative) by re-wrapping the LHS.
- Control-flow nodes allocate structured AST (if / while / for) with branches & optional pieces.
- For-loop supports optional init / condition / post segments similar to C.
- Blocks collect statements in a growable array.


### Error Handling
- ParseError structure with message + position; first fatal error stops parsing.
- Some convenience flags (underline previous token line) for tighter diagnostic anchoring.

### Testing Harness
- Embedded suite in `main.c`; each test writes a temporary file, runs the front-end, checks expected success/failure.
- Compact summary: only unexpected results print source snippet.
- **Type system testing**: Extensive test coverage for complex type expressions, precedence, and edge cases.
- **AST validation**: Tests verify that the hierarchical AST output correctly represents the parsed structure.

## Debugging Features

The compiler provides several debugging options:

- `--tokens`: Display the token stream from lexical analysis
- `--ast`: Display the hierarchical AST structure with proper indentation
- `--time`: Show timing information for each compilation phase
- `--show-errors`: Display detailed error messages with line/column information

The hierarchical AST output is particularly useful for understanding how complex type expressions are parsed and represented internally.

## Grammar vs Implementation Notes
- **Type grammar compliance**: The parser now fully implements the BNF grammar for types, supporting:
  - All precedence levels: base types, function types, suffixes (arrays/pointers), grouping
  - Left-to-right suffix processing for correct precedence
  - Both `fn(...)` syntax and parenthesized type expressions
- Function call treated as expression (no separate `<FunctionCall>` statement rule internally).

## Example
Source: prog.lang
```c
fn add(a: i32, b: i32) -> i32 { return a + b; }
fn main() { 
    x: i32 = 0; 
    arr: i32[10]*;
    callback: fn(i32) -> i32;
    for (i: i32 = 0; i < 10; i = i + 1) { x += i; } 
}
```
Run:
```
./compiler-new prog.lang --ast
```
Outputs a hierarchical AST tree dump showing the complete structure:

```yaml
=== AST ===
Program:
  Function: add
    ReturnType:
      Type:
        BaseType: i32
    Parameters:
      Param: a
        Type:
          BaseType: i32
      Param: b
        Type:
          BaseType: i32
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
              BaseType: i32
            Initializer:
              Literal: Integer: 0
        Declaration:
          Variable: arr
            Type:
              BaseType: i32
              ArrayDimensions:
                Dimension[0]:
                  Literal: Integer: 10
```


## Type System
The compiler now supports a rich type system with proper precedence and grouping:
Semantic analysis to be implemented

### Base Types
- Primitive types: `i32`, `i64`, `f32`, `f64`, `bool`

### Complex Types
- **Function types**: `fn(param_types) -> return_type`
  - Examples: `fn(i32, i32) -> i32`, `fn() -> i32`, `fn(i32) -> fn(i32) -> i32`
- **Array types**: `type[dimension]` with expression dimensions
  - Examples: `i32[10]`, `f64[x + 1]`, `bool[factorial(5)]`
- **Pointer types**: `type*` with proper precedence
  - Examples: `i32*`, `i32**`, `fn(i32) -> i32*`


### Type Precedence (highest to lowest)
1. Base types and parenthesized expressions
2. Function types `fn(...) -> ...`
3. Suffixes (arrays `[...]` and pointers `*`) - left-to-right
4. Grouping with parentheses

### Examples of Complex Types
```c
// Function returning pointer to array of integers
fn get_array() -> i32[10]*

// Array of function pointers
callback_list: (fn(i32) -> i32)[100]

// Pointer to array vs array of pointers
ptr_to_array: (i32[10])*     // pointer to array
array_of_ptrs: i32*[10]      // array of pointers

// Nested function types
higher_order: fn(fn(i32) -> i32) -> fn(i32) -> i32
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
- No type checking / symbol resolution yet (types are parsed but not validated)
- Minimal diagnostics & no recovery
- No codegen / optimization
- Array dimensions accept any expression but aren't evaluated at compile time
