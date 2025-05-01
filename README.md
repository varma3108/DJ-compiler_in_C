# DJ-compiler_in_C
Compiler with LALR(1) parsing, codegen, AST gen 
# DJ Compiler

A toy compiler for the DJ programming language, implemented in C using Flex and Bison. The compiler supports all stages from lexical analysis through code generation and includes a simple garbage collector for memory management.

## Features

- **Lexical Analysis**: Tokenization using Flex
- **Parsing**: LALR(1) grammar implemented in Bison
- **Abstract Syntax Tree (AST)**: Dynamic AST construction for classes, methods, expressions, and control flow
- **Symbol Table & Type Checking**:
  - Class, method, field, and local scopes
  - Static type enforcement, subtyping, overloading, and final modifiers
- **Code Generation**: Emits DISM bytecode for execution on a stack-based VM
  - Dynamic dispatch via v-tables
  - Object allocation and initialization
- **Garbage Collection**: Mark-and-sweep collector integrated into runtime
- **Testing**: Comprehensive suite of good/error DJ programs for validating compiler correctness

## Requirements

- C compiler (e.g., `gcc`)
- Flex
- Bison

## Building

```bash
flex dj.l
bison -v dj.y
sed -i '/extern YYSTYPE yylval/d' dj.tab.c
gcc dj.tab.c ast.c symtbl.c typecheck.c codegen.c -o djc
```

## Usage

```bash
./djc <source.dj>
```

- Generates `.dism` bytecode files
- Reports lexical, syntax, and semantic errors

## Private Repository Notice

The source code for this project has been privatized due to institutional policies. If you would like explanations of specific implementation details, code snippets, or have any questions, please feel free to email me at **indukuri3@usf.edu**.

---

*Thank you for your interest in the DJ Compiler!*

