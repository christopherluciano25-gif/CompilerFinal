/* MINIMAL C COMPILER - EDUCATIONAL VERSION
 * Demonstrates all phases of compilation with a simple language
 * Supports: int variables, addition, assignment, print
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>    /* for compilation timing (clock_gettime) */
#include "ast.h"
#include "semantic.h"
#include "codegen.h"
#include "tac.h"

extern int yyparse();
extern FILE* yyin;
extern ASTNode* root;

/* Helper function to generate TAC filenames from output filename */
void getTACFilename(const char* outputFile, char* tacFile, char* optimizedTacFile) {
    // Find the last dot in the filename
    const char* dot = strrchr(outputFile, '.');
    const char* slash = strrchr(outputFile, '/');

    if (dot && (!slash || dot > slash)) {
        // Copy everything before the extension
        int baseLen = dot - outputFile;
        strncpy(tacFile, outputFile, baseLen);
        tacFile[baseLen] = '\0';
        strcat(tacFile, ".tac");

        strncpy(optimizedTacFile, outputFile, baseLen);
        optimizedTacFile[baseLen] = '\0';
        strcat(optimizedTacFile, ".optimized.tac");
    } else {
        // No extension found, just append
        sprintf(tacFile, "%s.tac", outputFile);
        sprintf(optimizedTacFile, "%s.optimized.tac", outputFile);
    }
}


/* ── SOURCE PREPROCESSOR: Modulo operator ─────────────────────────────
 * The LALR parser tables were compiled without a % rule.
 * We handle this by transforming the source text before parsing:
 *
 *   1. Prepend a __mod helper function to the source
 *   2. Replace every token-level "%" with ", __MOD_MARKER_" so we can
 *      identify the split point, then reconstruct __mod(LHS, RHS)
 *
 * Simpler approach: scan the source character by character.
 * When we see  <expr> % <expr>  we need to rewrite as __mod(<expr>,<expr>)
 * This requires balanced-paren tracking and whitespace handling.
 *
 * PRACTICAL APPROACH used here:
 * We scan for the pattern:
 *   [identifier/number/closing-paren/bracket]  WHITESPACE*  %  WHITESPACE*  [identifier/number/opening-paren]
 * and rewrite each occurrence as __mod(LHS_TOKEN, RHS_TOKEN).
 * For compound expressions (a+b % c+d) the % has highest precedence in C,
 * so LHS is always a single atom (number, id, or parenthesized expression).
 * We collect the full LHS atom and RHS atom.
 */

/* Helper: check if char is part of an identifier or number */
static int isAtomChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* Preprocess source: rewrite a%b as __mod(a,b), write to tmpfile.
 * Returns the path to the temp file (static buffer). */
static const char* preprocessModulo(const char* inputPath) {
    static char tmpPath[256];
    snprintf(tmpPath, sizeof(tmpPath), "/tmp/pp_%s", strrchr(inputPath,'/') ?
             strrchr(inputPath,'/')+1 : inputPath);

    FILE* in = fopen(inputPath, "r");
    if (!in) return inputPath;          /* can't open, use original */

    /* read entire file */
    fseek(in, 0, SEEK_END);
    long sz = ftell(in);
    rewind(in);
    char* src = malloc(sz + 1);
    fread(src, 1, sz, in);
    src[sz] = '\0';
    fclose(in);

    int needsPreprocess = 0;
    for (long i = 0; i < sz - 1; i++) {
        if (src[i] == '%') { needsPreprocess = 1; break; }
        if (src[i] == '&' && src[i+1] == '&') { needsPreprocess = 1; break; }
        if (src[i] == '|' && src[i+1] == '|') { needsPreprocess = 1; break; }
        if (src[i] == '!' && src[i+1] != '=') { needsPreprocess = 1; break; }
    }

    if (!needsPreprocess) {
        free(src);
        return inputPath;
    }

    printf("    [PREPROCESSOR] Found operators — injecting helpers and rewriting\n");

    /* Build output: inject __mod at top, then rewrite % occurrences */
    /* We use a dynamic output buffer */
    long outCap = sz * 4 + 1024;
    char* out   = malloc(outCap);
    long  opos  = 0;

#define EMIT(c) do { if (opos >= outCap-2) { outCap *= 2; out = realloc(out, outCap); } out[opos++] = (c); } while(0)
#define EMITS(s) do { const char* _s = (s); while(*_s) EMIT(*_s++); } while(0)

    /* Inject __mod helper function before user code */
    EMITS("int __mod(int a, int b) { int q; q = a / b; return a - q * b; }\n");
    EMITS("int __and(int a, int b) { if (a) { if (b) { return 1; } } return 0; }\n");
    EMITS("int __or(int a, int b)  { if (a) { return 1; } if (b) { return 1; } return 0; }\n");
    EMITS("int __not(int a)        { if (a) { return 0; } return 1; }\n");

    long i = 0;
    while (i < sz) {
        /* skip single-line comments */
        if (i+1 < sz && src[i] == '/' && src[i+1] == '/') {
            while (i < sz && src[i] != '\n') EMIT(src[i++]);
            continue;
        }
        /* skip multi-line comments */
        if (i+1 < sz && src[i] == '/' && src[i+1] == '*') {
            EMIT(src[i++]); EMIT(src[i++]);
            while (i+1 < sz && !(src[i] == '*' && src[i+1] == '/'))
                EMIT(src[i++]);
            if (i+1 < sz) { EMIT(src[i++]); EMIT(src[i++]); }
            continue;
        }
        /* skip string literals */
        if (src[i] == '"') {
            EMIT(src[i++]);
            while (i < sz && src[i] != '"') {
                if (src[i] == '\\') EMIT(src[i++]);
                EMIT(src[i++]);
            }
            if (i < sz) EMIT(src[i++]);
            continue;
        }

        /* look for && or || ONLY when inside parentheses — scan full condition
         * We detect the pattern by looking ahead in src for the full paren group.
         * Actually: we collect the LHS by walking BACK in the output to find
         * the start of the comparison expression (includes id, spaces, ops like > < == !=).
         */
        if (i+1 < sz && src[i] == '&' && src[i+1] == '&') {
            /* Walk back in output to find full LHS comparison expression.
             * LHS comparison can contain: identifiers, numbers, spaces, > < = ! */
            long lhsTail = opos - 1;
            while (lhsTail >= 0 && (out[lhsTail] == ' ' || out[lhsTail] == '\t'))
                lhsTail--;

            /* Walk back past comparison (id/num/ops/spaces) stopping at ( ; { , = */
            long lhsStart = lhsTail;
            while (lhsStart > 0) {
                char c = out[lhsStart - 1];
                /* stop at statement/expression boundary characters */
                if (c == '(' || c == '{' || c == ';' || c == ',' || c == '\n') break;
                /* stop at assignment = but not == or <= >= != */
                if (c == '=' && lhsStart >= 2) {
                    char prev = out[lhsStart - 2];
                    if (prev != '<' && prev != '>' && prev != '!' && prev != '=') break;
                }
                lhsStart--;
            }
            /* trim leading spaces */
            while (lhsStart <= lhsTail && (out[lhsStart] == ' ' || out[lhsStart] == '\t'))
                lhsStart++;

            long lhsLen = lhsTail - lhsStart + 1;
            char* lhs = malloc(lhsLen + 1);
            memcpy(lhs, out + lhsStart, lhsLen);
            lhs[lhsLen] = '\0';
            /* trim trailing spaces from lhs */
            long tl = lhsLen - 1;
            while (tl > 0 && (lhs[tl] == ' ' || lhs[tl] == '\t')) tl--;
            lhs[tl+1] = '\0';
            opos = lhsStart;

            i += 2; /* skip && */
            while (i < sz && (src[i] == ' ' || src[i] == '\t')) i++;

            /* Collect RHS: everything until ) ; { at depth 0 */
            char rhs[512]; int ri = 0;
            int pdepth = 0;
            while (i < sz) {
                char c = src[i];
                if ((c == ')' || c == ';' || c == '{') && pdepth == 0) break;
                if (c == '(') pdepth++;
                else if (c == ')') pdepth--;
                /* stop if we hit another && or || */
                if (c == '&' && i+1 < sz && src[i+1] == '&') break;
                if (c == '|' && i+1 < sz && src[i+1] == '|') break;
                rhs[ri++] = src[i++];
            }
            /* trim trailing spaces */
            while (ri > 0 && (rhs[ri-1] == ' ' || rhs[ri-1] == '\t')) ri--;
            rhs[ri] = '\0';

            EMITS("__and("); EMITS(lhs); EMITS(", "); EMITS(rhs); EMIT(')');
            printf("    [PREPROCESSOR] Rewrote: (%s) && (%s) → __and(%s, %s)\n", lhs, rhs, lhs, rhs);
            free(lhs);
            continue;
        }

        if (i+1 < sz && src[i] == '|' && src[i+1] == '|') {
            long lhsTail = opos - 1;
            while (lhsTail >= 0 && (out[lhsTail] == ' ' || out[lhsTail] == '\t'))
                lhsTail--;

            long lhsStart = lhsTail;
            while (lhsStart > 0) {
                char c = out[lhsStart - 1];
                if (c == '(' || c == '{' || c == ';' || c == ',' || c == '\n') break;
                if (c == '=' && lhsStart >= 2) {
                    char prev = out[lhsStart - 2];
                    if (prev != '<' && prev != '>' && prev != '!' && prev != '=') break;
                }
                lhsStart--;
            }
            while (lhsStart <= lhsTail && (out[lhsStart] == ' ' || out[lhsStart] == '\t'))
                lhsStart++;

            long lhsLen = lhsTail - lhsStart + 1;
            char* lhs = malloc(lhsLen + 1);
            memcpy(lhs, out + lhsStart, lhsLen);
            lhs[lhsLen] = '\0';
            long tl = lhsLen - 1;
            while (tl > 0 && (lhs[tl] == ' ' || lhs[tl] == '\t')) tl--;
            lhs[tl+1] = '\0';
            opos = lhsStart;

            i += 2; /* skip || */
            while (i < sz && (src[i] == ' ' || src[i] == '\t')) i++;

            char rhs[512]; int ri = 0;
            int pdepth = 0;
            while (i < sz) {
                char c = src[i];
                if ((c == ')' || c == ';' || c == '{') && pdepth == 0) break;
                if (c == '(') pdepth++;
                else if (c == ')') pdepth--;
                if (c == '&' && i+1 < sz && src[i+1] == '&') break;
                if (c == '|' && i+1 < sz && src[i+1] == '|') break;
                rhs[ri++] = src[i++];
            }
            while (ri > 0 && (rhs[ri-1] == ' ' || rhs[ri-1] == '\t')) ri--;
            rhs[ri] = '\0';

            EMITS("__or("); EMITS(lhs); EMITS(", "); EMITS(rhs); EMIT(')');
            printf("    [PREPROCESSOR] Rewrote: (%s) || (%s) → __or(%s, %s)\n", lhs, rhs, lhs, rhs);
            free(lhs);
            continue;
        }

        /* look for ! (logical NOT) — only when followed by identifier or ( */
        if (src[i] == '!' && i+1 < sz && src[i+1] != '=') {
            i++; /* skip ! */
            while (i < sz && (src[i] == ' ' || src[i] == '\t')) i++;

            char operand[256]; int oi = 0;
            if (src[i] == '(') {
                int depth = 1; operand[oi++] = src[i++];
                while (i < sz && depth > 0) {
                    if (src[i] == '(') depth++;
                    else if (src[i] == ')') depth--;
                    operand[oi++] = src[i++];
                }
            } else {
                while (i < sz && isAtomChar(src[i])) operand[oi++] = src[i++];
            }
            operand[oi] = '\0';

            EMITS("__not("); EMITS(operand); EMIT(')');
            printf("    [PREPROCESSOR] Rewrote: !%s → __not(%s)\n", operand, operand);
            continue;
        }

        /* look for % operator */
        if (src[i] == '%') {
            /* We need to find the LHS atom — it ends right before this %.
             * Walk back through output to find the start of the LHS atom.
             * LHS atom is: identifier, number, or closing ) or ]
             * If LHS ends with ) or ], we need to find the matching open.
             */
            long lhsEnd = opos;   /* points past last char of LHS in out[] */

            /* strip trailing whitespace from output to find LHS end */
            long lhsTail = lhsEnd - 1;
            while (lhsTail >= 0 && (out[lhsTail] == ' ' || out[lhsTail] == '\t'))
                lhsTail--;

            long lhsStart;
            if (lhsTail >= 0 && out[lhsTail] == ')') {
                /* walk back to matching ( */
                int depth = 1; lhsStart = lhsTail - 1;
                while (lhsStart >= 0 && depth > 0) {
                    if (out[lhsStart] == ')') depth++;
                    else if (out[lhsStart] == '(') depth--;
                    lhsStart--;
                }
                lhsStart++;  /* points at '(' */
            } else if (lhsTail >= 0 && out[lhsTail] == ']') {
                /* walk back to matching [ */
                int depth = 1; lhsStart = lhsTail - 1;
                while (lhsStart >= 0 && depth > 0) {
                    if (out[lhsStart] == ']') depth++;
                    else if (out[lhsStart] == '[') depth--;
                    lhsStart--;
                }
                lhsStart++;
            } else {
                /* identifier or number: walk back while isAtomChar */
                lhsStart = lhsTail;
                while (lhsStart > 0 && isAtomChar(out[lhsStart-1]))
                    lhsStart--;
            }

            /* extract LHS string */
            long lhsLen = lhsTail - lhsStart + 1;
            char* lhs = malloc(lhsLen + 1);
            memcpy(lhs, out + lhsStart, lhsLen);
            lhs[lhsLen] = '\0';

            /* remove LHS from output buffer */
            opos = lhsStart;

            /* skip whitespace after % in source */
            i++;  /* skip the % */
            while (i < sz && (src[i] == ' ' || src[i] == '\t')) i++;

            /* collect RHS atom from source */
            char rhs[256]; int ri = 0;
            if (src[i] == '(') {
                /* parenthesized expression */
                int depth = 1; rhs[ri++] = src[i++];
                while (i < sz && depth > 0) {
                    if (src[i] == '(') depth++;
                    else if (src[i] == ')') depth--;
                    rhs[ri++] = src[i++];
                }
            } else {
                /* identifier or number */
                while (i < sz && isAtomChar(src[i]))
                    rhs[ri++] = src[i++];
            }
            rhs[ri] = '\0';

            /* emit __mod(lhs, rhs) */
            EMITS("__mod(");
            EMITS(lhs);
            EMITS(", ");
            EMITS(rhs);
            EMIT(')');

            printf("    [PREPROCESSOR] Rewrote: %s %% %s → __mod(%s, %s)\n",
                   lhs, rhs, lhs, rhs);

            free(lhs);
            continue;
        }

        EMIT(src[i++]);
    }
    out[opos] = '\0';

    /* write preprocessed source to temp file */
    FILE* tmp = fopen(tmpPath, "w");
    if (!tmp) { free(src); free(out); return inputPath; }
    fwrite(out, 1, opos, tmp);
    fclose(tmp);

    free(src);
    free(out);
    return tmpPath;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input.c> <output.s>\n", argv[0]);
        printf("Example: ./minicompiler test.c output.s\n");
        return 1;
    }

    /* ── PERFORMANCE METRIC: Compilation Start Time ─────────────────
     * We record the wall-clock time before any compilation work begins.
     * At the end of main() we subtract to get total compilation time.
     * clock_gettime(CLOCK_MONOTONIC) is used for high-resolution timing
     * that is immune to system clock adjustments.
     */
    struct timespec compile_start, compile_end;
    clock_gettime(CLOCK_MONOTONIC, &compile_start);

    /* Preprocess source: rewrite % modulo operator as __mod() calls */
    const char* sourceFile = preprocessModulo(argv[1]);

    yyin = fopen(sourceFile, "r");
    if (!yyin) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", argv[1]);
        return 1;
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          MINIMAL C COMPILER - EDUCATIONAL VERSION         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* PHASE 1: Lexical and Syntax Analysis */
    printf("┌──────────────────────────────────────────────────────────┐\n");
    printf("│ PHASE 1: LEXICAL & SYNTAX ANALYSIS                       │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│ • Reading source file: %s\n", argv[1]);
    printf("│ • Tokenizing input (scanner.l)\n");
    printf("│ • Parsing grammar rules (parser.y)\n");
    printf("│ • Building Abstract Syntax Tree\n");
    printf("└──────────────────────────────────────────────────────────┘\n");
    
    if (yyparse() == 0) {
        printf("✓ Parse successful - program is syntactically correct!\n\n");
        
        /* PHASE 2: AST Display */
        printf("┌──────────────────────────────────────────────────────────┐\n");
        printf("│ PHASE 2: ABSTRACT SYNTAX TREE (AST)                      │\n");
        printf("├──────────────────────────────────────────────────────────┤\n");
        printf("│ Tree structure representing the program hierarchy:        │\n");
        printf("└──────────────────────────────────────────────────────────┘\n");
        printAST(root, 0);
        printf("\n");

        /* PHASE 3: Semantic Analysis */
        printf("┌──────────────────────────────────────────────────────────┐\n");
        printf("│ PHASE 3: SEMANTIC ANALYSIS                               │\n");
        printf("├──────────────────────────────────────────────────────────┤\n");
        printf("│ Checking program semantics:                              │\n");
        printf("│ • Variable declarations and usage                        │\n");
        printf("│ • Type compatibility                                     │\n");
        printf("│ • Duplicate declarations                                 │\n");
        printf("└──────────────────────────────────────────────────────────┘\n");
        initSemantic();
        int semResult = performSemanticAnalysis(root);
        printSemanticSummary();

        if (semResult != 0) {
            printf("✗ Compilation failed due to semantic errors\n");
            fclose(yyin);
            return 1;
        }

        /* PHASE 4: Intermediate Code */
        printf("┌──────────────────────────────────────────────────────────┐\n");
        printf("│ PHASE 4: INTERMEDIATE CODE GENERATION                    │\n");
        printf("├──────────────────────────────────────────────────────────┤\n");
        printf("│ Three-Address Code (TAC) - simplified instructions:       │\n");
        printf("│ • Each instruction has at most 3 operands                │\n");
        printf("│ • Temporary variables (t0, t1, ...) for expressions      │\n");
        printf("└──────────────────────────────────────────────────────────┘\n");
        initTAC();
        generateTAC(root);
        printTAC();
        printTempAllocatorState();

        // Generate TAC filename and save to file
        char tacFile[256], optimizedTacFile[256];
        getTACFilename(argv[2], tacFile, optimizedTacFile);
        saveTACToFile(tacFile);

        /* PHASE 5: Optimization */
        printf("┌──────────────────────────────────────────────────────────┐\n");
        printf("│ PHASE 5: CODE OPTIMIZATION                               │\n");
        printf("├──────────────────────────────────────────────────────────┤\n");
        printf("│ Applying optimizations:                                  │\n");
        printf("│ • Constant folding (evaluate compile-time expressions)   │\n");
        printf("│ • Copy propagation (replace variables with values)       │\n");
        printf("└──────────────────────────────────────────────────────────┘\n");
        optimizeTAC();
        printOptimizedTAC();
        printf("\n");

        // Save optimized TAC to file
        saveOptimizedTACToFile(optimizedTacFile);

        /* PHASE 6: Code Generation */
        printf("┌──────────────────────────────────────────────────────────┐\n");
        printf("│ PHASE 6: MIPS CODE GENERATION                            │\n");
        printf("├──────────────────────────────────────────────────────────┤\n");
        printf("│ Translating OPTIMIZED TAC to MIPS assembly:              │\n");
        printf("│ • Using optimized TAC (with constant folding)            │\n");
        printf("│ • Variables stored on stack                              │\n");
        printf("│ • Register allocation with LRU spilling                  │\n");
        printf("│ • System calls for print operations                      │\n");
        printf("└──────────────────────────────────────────────────────────┘\n");
        generateMIPSFromTAC(argv[2]);
        printf("✓ MIPS assembly code generated to: %s\n", argv[2]);
        printf("\n");
        
        printf("╔════════════════════════════════════════════════════════════╗\n");
        printf("║                  COMPILATION SUCCESSFUL!                   ║\n");
        printf("║         Run the output file in a MIPS simulator           ║\n");
        printf("╚════════════════════════════════════════════════════════════╝\n");
    } else {
        printf("✗ Parse failed - check your syntax!\n");
        printf("Common errors:\n");
        printf("  • Missing semicolon after statements\n");
        printf("  • Undeclared variables\n");
        printf("  • Invalid syntax for print statements\n");
        return 1;
    }
    
    fclose(yyin);

    /* ── PERFORMANCE METRIC: Compilation End Time ────────────────────
     * Record end time and compute total compilation duration.
     * We report:
     *   - Total compilation time in milliseconds
     *   - Estimated execution time note (MIPS runs in simulator)
     * This satisfies the performance metrics requirement for Project 6.
     */
    clock_gettime(CLOCK_MONOTONIC, &compile_end);

    double compile_ms = (compile_end.tv_sec  - compile_start.tv_sec)  * 1000.0
                      + (compile_end.tv_nsec - compile_start.tv_nsec) / 1000000.0;

    printf("\n");
    printf("┌──────────────────────────────────────────────────────────┐\n");
    printf("│ PERFORMANCE METRICS                                      │\n");
    printf("├──────────────────────────────────────────────────────────┤\n");
    printf("│ Compilation time:  %.3f ms                              │\n", compile_ms);
    printf("│ Source file:       %s                                    │\n", argv[1]);
    printf("│ Output file:       %s                                    │\n", argv[2]);
    printf("│                                                          │\n");
    printf("│ Execution time:    N/A (MIPS assembly runs in QTSpim)   │\n");
    printf("│ To time execution: load output.s in QTSpim/MARS and     │\n");
    printf("│ use the built-in simulator step counter or wall clock.   │\n");
    printf("└──────────────────────────────────────────────────────────┘\n");

    return 0;
}