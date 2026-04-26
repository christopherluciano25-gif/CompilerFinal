# CST-405 Mini C Compiler

A complete educational compiler that translates a subset of C into MIPS assembly language. Built across six projects for CST-405 Compiler Design at Grand Canyon University.

---

## Features

- **Lexical analysis** — tokenizes source code (scanner.l)
- **Syntax analysis** — validates grammar and builds an Abstract Syntax Tree (parser.y)
- **Semantic analysis** — checks variable declarations, function calls, and type usage (semantic.c)
- **Intermediate code generation** — produces Three-Address Code (TAC) from the AST (tac.c)
- **Code optimization** — constant folding and copy propagation (tac.c)
- **MIPS code generation** — translates optimized TAC to MIPS assembly with LRU register allocation (codegen.c)
- **Performance metrics** — reports compilation time in milliseconds after every run

### Supported Language Features

| Feature | Example |
|---|---|
| Integer variables | `int x;` |
| Assignment | `x = 5;` |
| Arithmetic | `x = a + b * c;` |
| Modulo | `x = a % b;` |
| Comparisons | `x > 3`, `x == y`, `x != 0` |
| Print | `print(x);` |
| Functions | `int add(int a, int b) { return a + b; }` |
| Arrays | `int arr[10]; arr[0] = 1;` |
| if / if-else | `if (x > 0) { ... } else { ... }` |
| Nested if | `if (x) { if (y) { ... } }` |
| Switch | `switch (x) { case 1: ... break; }` |
| while loop | `while (i < 10) { i = i + 1; }` |
| for loop | `for (i = 0; i < 10; i = i + 1) { ... }` |
| do-while loop | `do { ... } while (cond);` |
| Logical AND | `if (a > 0 && b > 0)` |
| Logical OR | `if (a > 0 \|\| b > 0)` |
| Logical NOT | `if (!x)` |

---

## Installation

### Prerequisites

- **GCC** — C compiler (version 7 or higher)
- **Make** — build tool
- **WSL (Windows)** or any Linux/macOS terminal

No Flex or Bison installation required — pre-generated scanner and parser C files are included.

### Windows (WSL)

```bash
# Open WSL terminal, navigate to the project
cd /mnt/c/Users/YourName/Desktop/CST-405-Project6

# Build the compiler
make
```

### Linux / macOS

```bash
cd CST-405-Project6
make
```

### Verify the build

```bash
ls -la minicompiler
```

You should see the `minicompiler` executable. If you see errors, ensure GCC is installed:

```bash
gcc --version
```

---

## Usage

### Basic usage

```bash
./minicompiler <input.c> <output.s>
```

### Example

```bash
./minicompiler test_decisions.c output.s
```

This compiles `test_decisions.c` through all six phases and writes MIPS assembly to `output.s`. It also saves:
- `output.tac` — unoptimized three-address code
- `output.optimized.tac` — optimized three-address code

### Running the output in a MIPS simulator

**QTSpim:**
1. Open QTSpim
2. File > Load File > select `output.s`
3. Click Run

**MARS:**
1. Open MARS
2. File > Open > select `output.s`
3. Click Assemble, then Run

### Available test files

| File | Description |
|---|---|
| `test_decisions.c` | Tests if/else, nested if, switch, &&, \|\|, ! |
| `test_loops.c` | Tests while, for, do-while, modulo, order of operations |
| `test_complete_working.c` | Tests functions, arrays, arithmetic |
| `test_semantic_errors.c` | Demonstrates 11 semantic error types |

Run all tests:

```bash
./minicompiler test_decisions.c test_decisions.s
./minicompiler test_loops.c test_loops.s
./minicompiler test_complete_working.c test_complete.s
```

### Cleaning build artifacts

```bash
make clean
```

---

## Performance Metrics

Every compilation run reports timing at the end:

```
┌──────────────────────────────────────────────────────────┐
│ PERFORMANCE METRICS                                      │
├──────────────────────────────────────────────────────────┤
│ Compilation time:  3.085 ms                              │
│ Source file:       test_decisions.c                      │
│ Output file:       test_decisions.s                      │
│                                                          │
│ Execution time:    N/A (MIPS assembly runs in QTSpim)   │
│ To time execution: load output.s in QTSpim/MARS and     │
│ use the built-in simulator step counter or wall clock.   │
└──────────────────────────────────────────────────────────┘
```

### Compilation time

Measured using `clock_gettime(CLOCK_MONOTONIC)` from the start of `main()` to the end of all six phases. Typical times on a modern machine:

| Input file | Compilation time |
|---|---|
| test_decisions.c (9 test cases) | ~3 ms |
| test_loops.c (6 test cases) | ~2 ms |
| test_complete_working.c | ~3 ms |

The compiler is fast because it makes a single pass through each phase — the scanner, parser, semantic analyzer, TAC generator, optimizer, and code generator each process the input exactly once.

### Execution time

The generated MIPS assembly runs inside QTSpim or MARS. Execution time depends on the simulator. For the test files:

- Simple programs (10–50 instructions) typically execute in under 1 ms of simulator time
- Loop-heavy programs (like sumRange(1000)) may take a few ms of simulator time due to repeated iteration

To measure execution time in MARS: use **Tools > Instruction Counter** and divide by your target clock speed. QTSpim does not have a built-in timer but the number of instructions executed can be read from the register display after stepping through.

---

## Project Structure

```
CST-405-Project6/
├── minicompiler          ← compiled executable (after make)
├── Makefile              ← build system
├── scanner.l             ← lexical analyzer (flex source)
├── parser.y              ← grammar rules (bison source)
├── lex_yy.c              ← pre-generated scanner (no flex needed)
├── parser_tab.c          ← pre-generated parser (no bison needed)
├── parser_tab.h          ← pre-generated parser header
├── parser.tab.h          ← patched header with all token definitions
├── loop_preprocessor.c  ← token interceptor: handles for, do-while, switch
├── main.c                ← compiler driver with timing/performance metrics
├── ast.h / ast.c         ← Abstract Syntax Tree node types and functions
├── symtab.h / symtab.c   ← symbol table with stack offset tracking
├── semantic.h / semantic.c ← semantic analyzer (scope checking, error reporting)
├── tac.h / tac.c         ← TAC generation and optimization
├── codegen.h / codegen.c ← MIPS code generator with LRU register allocation
├── test_decisions.c      ← Project 5 test: if/switch/logical operators
├── test_loops.c          ← Project 4 test: loops and order of operations
├── test_complete_working.c ← Project 3 test: functions and arrays
└── test_semantic_errors.c  ← semantic error detection test
```

---

## How It Works (Compiler Phases)

1. **Phase 1 — Lexical & Syntax Analysis**: The source file is preprocessed (rewriting `&&`, `||`, `!`, `%`, and `switch` into forms the parser understands), then tokenized and parsed into an AST.
2. **Phase 2 — AST Display**: The full parse tree is printed showing the program hierarchy.
3. **Phase 3 — Semantic Analysis**: The AST is traversed checking declarations, scopes, and function call correctness. Errors are reported with line numbers.
4. **Phase 4 — TAC Generation**: Three-address code is generated from the AST. Each instruction has at most three operands.
5. **Phase 5 — Optimization**: Constant folding and copy propagation are applied to the TAC.
6. **Phase 6 — MIPS Generation**: The optimized TAC is translated to MIPS assembly using an LRU register allocator.

---

## Author

Christopher Luciano — Grand Canyon University, CST-405 Compiler Design
