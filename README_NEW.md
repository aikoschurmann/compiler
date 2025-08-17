# Custom Programming Language Compiler

A C-based compiler implementation for a custom programming language featuring lexical analysis, parsing, and AST generation.

## Project Overview

This project implements a complete compiler frontend with:
- **Lexical Analysis**: Token generation with regex and fixed-string matching
- **Syntax Analysis**: Recursive descent parser with error recovery
- **AST Generation**: Complete Abstract Syntax Tree construction
- **Type System**: Support for primitive types, arrays, and pointers

## Language Features

### Supported Types
- **Integers**: `i32`, `i64`
- **Floating Point**: `f32`, `f64`
- **Boolean**: `bool` (with `true`/`false` literals)
- **Character**: `char`
- **Arrays**: Multi-dimensional with compile-time and runtime sizing
- **Pointers**: Multiple levels of indirection
- **Constants**: `const` type qualifier

### Operators
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logical**: `&&`, `||`, `!`
- **Assignment**: `=`, `+=`, `-=`, `*=`, `/=`, `%=`
- **Unary**: `+`, `-`, `!`, `&` (address-of), `*` (dereference)
- **Increment/Decrement**: `++`, `--` (both prefix and postfix)

### Language Constructs
- **Variable Declarations**: `name: [const] type [= initializer];`
- **Function Declarations**: `fn name(params) [-> return_type] { ... }`
- **Control Flow**: `if`/`else`, `while`, `for` loops
- **Statements**: `return`, `break`, `continue`
- **Expressions**: Full operator precedence and associativity
- **Array Access**: `array[index]` syntax
- **Function Calls**: `function(args)` syntax

## Project Structure

```
compiler-new/
├── include/           # Header files
│   ├── ast.h         # AST node definitions
│   ├── lexer.h       # Lexer interface
│   ├── parser.h      # Parser interface
│   ├── token.h       # Token definitions
│   └── tokens.def    # Token specification (X-macro)
├── src/              # Source files
│   ├── ast.c         # AST creation and manipulation
│   ├── ast_parse_statements.c  # Parser implementation
│   ├── file.c        # File I/O utilities
│   ├── lexer.c       # Lexical analyzer
│   ├── main.c        # Main compiler driver
│   ├── parser.c      # Parser utilities
│   └── token.c       # Token utilities
├── input/            # Test files and grammar
│   ├── lang.bnf      # Language grammar specification
│   └── test.txt      # Sample source code
├── obj/              # Compiled object files
└── compiler-steps/   # Debug output (tokens, AST)
```

## Building and Running

### Prerequisites
- GCC compiler with C99 support
- Make
- pkg-config (for dependencies)

### Build Commands
```bash
# Build the compiler
make

# Clean build artifacts
make clean

# Run the compiler on test input
./out/compiler
```

The compiler reads from `./input/test.txt` by default and outputs:
1. Colored token stream
2. Parse results (success/error messages)
3. Pretty-printed AST (if parsing succeeds)

## Grammar Specification

The language follows this BNF grammar (see `input/lang.bnf` for complete specification):

```bnf
<Program> ::= { <Declaration> }

<Declaration> ::= <VariableDeclarationStmt> | <FunctionDeclaration>

<VariableDeclaration> ::= 
    IDENTIFIER COLON [ CONST ] <Type> [ ASSIGN ( <Expression> | <InitializerList> ) ]

<Expression> ::= <Assignment> | <LogicalOr>

<Assignment> ::= <Lvalue> <AssignOp> <Expression>

<LogicalOr>  ::= <LogicalAnd> { OR_OR <LogicalAnd> }
<LogicalAnd> ::= <Equality>  { AND_AND <Equality> }
<Equality>   ::= <Relational> { ( EQ_EQ | BANG_EQ ) <Relational> }
<Relational> ::= <Additive>   { ( LT | GT | LT_EQ | GT_EQ ) <Additive> }
<Additive>   ::= <Multiplicative> { ( PLUS | MINUS ) <Multiplicative> }
<Multiplicative> ::= <Unary> { ( STAR | SLASH | PERCENT ) <Unary> }

<Unary>   ::= <PrefixOp> <Unary> | <Postfix>
<Postfix> ::= <Primary> { <PostfixOp> }
<Primary> ::= INTEGER | FLOAT | BOOLEAN | IDENTIFIER | LPAREN <Expression> RPAREN
```

## Token Definitions

Tokens are defined in `tokens.def` using an X-macro format:

    X(NAME, literal-comparison, regex_pattern)

- **NAME**: The enum name for the token type
- **literal-comparison**: Fixed string that the lexer matches directly
- **regex_pattern**: Regular expression for complex tokens (identifiers, numbers, strings)

## Lexer Operation

1. **Whitespace Skipping**: Advances past whitespace, tracking line/column positions
2. **Comment Handling**: Recognizes `//` line comments
3. **Fixed String Matching**: Matches keywords and operators by exact spelling
4. **Regex Matching**: Handles identifiers, numbers, and string literals
5. **Error Recovery**: Unknown tokens are consumed as single characters

## Parser Features

- **Recursive Descent**: Hand-written parser following grammar rules
- **Error Recovery**: Detailed error messages with position information
- **Operator Precedence**: Correct associativity and precedence handling
- **AST Construction**: Builds complete abstract syntax tree
- **Memory Management**: Proper cleanup on parse errors

## AST Node Types

The compiler generates these AST node types:
- **Declarations**: Program, Variable, Function, Parameters
- **Statements**: Block, If, While, For, Return, Break, Continue, Expression
- **Expressions**: Literal, Identifier, Binary, Unary, Assignment, Call, Subscript
- **Types**: Base types with array dimensions and pointer levels

## Example Usage

Sample input (`input/test.txt`):
```
// Variable declaration with array type
nested: i32*[1 + 1];
```

This declares a variable `nested` of type "pointer to array of 2 i32s".

## Error Messages

The compiler provides detailed error messages with:
- **Location Information**: Line and column numbers
- **Context**: What was expected vs. what was found
- **Recovery**: Parser attempts to continue after errors

Example error output:
```
Error at line 2, column 15: expected ';' after variable declaration
```

## Current Status

- ✅ Lexical analysis complete
- ✅ Basic expression parsing  
- ✅ Variable declarations
- ✅ Operator precedence and associativity
- 🚧 Function declarations (placeholder)
- 🚧 Statement parsing (control flow)
- 🚧 Type checking and validation
- ❌ Code generation and optimization

## Architecture Notes

### Memory Management
- All AST nodes are dynamically allocated
- Parse errors trigger proper cleanup of partial ASTs
- Token arrays manage their own memory

### Error Handling
- Parse errors are collected in `ParseError` structures
- Parser attempts to recover and continue parsing
- Multiple errors can be reported in a single pass

### Extensibility
- Grammar rules are easily added to the recursive descent parser
- New token types can be added to `tokens.def`
- AST node types are extensible through the union structure

## Contributing

The codebase is structured for easy extension. To add new features:

1. **New Tokens**: Add entries to `tokens.def`
2. **Grammar Rules**: Implement parsing functions in `ast_parse_statements.c`
3. **AST Nodes**: Define new node types in `ast.h` and implement in `ast.c`
4. **Type System**: Extend type parsing and semantic analysis

## Testing

Test your changes with:
1. Modify `input/test.txt` with sample code
2. Run `make && ./out/compiler`
3. Check token output and AST structure
4. Verify error handling with invalid syntax

## License

This project is for educational and research purposes. Individual files may have specific licensing terms.
