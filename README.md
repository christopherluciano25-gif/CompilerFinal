CST-405 Mini C Compiler
A complete six-phase educational compiler that translates a subset of C into MIPS assembly language. Built for CST-405 Compiler Design at Grand Canyon University.
Features

Integer variables, arrays, and functions
Arithmetic with correct order of operations (+, -, *, /, %)
While, for, and do-while loops
If-then, if-else, nested if, and switch statements
Logical operators: && (AND), || (OR), ! (NOT)
Six compiler phases: scan → parse → AST → semantic analysis → TAC → MIPS

How to Build
Requires GCC and Make. No Flex or Bison needed.
make
How to Run
./minicompiler input.c output.s
Test Files
./minicompiler test_decisions.c output.s
./minicompiler test_loops.c output.s
./minicompiler test_complete_working.c output.s
Output
The compiler produces:

output.s — MIPS assembly (run in QTSpim or MARS)
output.tac — unoptimized three-address code
output.optimized.tac — optimized three-address code
Compilation time printed to terminal after every run
