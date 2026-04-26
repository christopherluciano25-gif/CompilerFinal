/* Project 4 Test: Loops and Order of Operations
 * Tests:
 *   1. while loop (existing)
 *   2. for loop (new)
 *   3. do-while loop (new)
 *   4. Order of operations (*, / before +, -, parentheses override)
 *   5. Modulo operator %
 *   6. Nested loops
 */

int main() {

    int i;
    int sum;
    int result;
    int x;

    /* ── TEST 1: WHILE LOOP ──────────────────────────────── */
    print(111);
    /* sum = 0+1+2+3+4 = 10 */
    sum = 0;
    i = 0;
    while (i < 5) {
        sum = sum + i;
        i = i + 1;
    }
    print(sum);

    /* ── TEST 2: FOR LOOP ────────────────────────────────── */
    print(222);
    /* sum = 1+2+3+4+5 = 15 */
    sum = 0;
    for (i = 1; i <= 5; i = i + 1) {
        sum = sum + i;
    }
    print(sum);

    /* ── TEST 3: DO-WHILE LOOP ───────────────────────────── */
    print(333);
    /* Body runs at least once even if condition starts false.
       sum = 0 + 10 = 10  (runs once then condition 10 < 5 is false) */
    sum = 0;
    i = 10;
    do {
        sum = sum + i;
        i = i + 1;
    } while (i < 5);
    print(sum);

    /* ── TEST 4: ORDER OF OPERATIONS ─────────────────────── */
    print(444);
    /* 2 + 3 * 4 = 14  (not 20 — * before +) */
    result = 2 + 3 * 4;
    print(result);

    /* (2 + 3) * 4 = 20  (parens override) */
    result = (2 + 3) * 4;
    print(result);

    /* 10 - 2 * 3 + 1 = 5  (2*3=6, 10-6=4, 4+1=5) */
    result = 10 - 2 * 3 + 1;
    print(result);

    /* 100 / 5 / 2 = 10  (left-associative: (100/5)/2) */
    result = 100 / 5 / 2;
    print(result);

    /* ── TEST 5: MODULO OPERATOR ─────────────────────────── */
    print(555);
    /* 10 % 3 = 1 */
    result = 10 % 3;
    print(result);

    /* 7 % 2 = 1  (odd check) */
    result = 7 % 2;
    print(result);

    /* mixed: 2 + 10 % 3 = 3  (% before +) */
    result = 2 + 10 % 3;
    print(result);

    /* ── TEST 6: NESTED FOR INSIDE WHILE ─────────────────── */
    print(666);
    /* outer while counts i 0..1, inner for counts j 1..2
       sum increments 2 times per outer iteration = 4 total */
    sum = 0;
    i = 0;
    while (i < 2) {
        for (x = 1; x <= 2; x = x + 1) {
            sum = sum + 1;
        }
        i = i + 1;
    }
    print(sum);

    return 0;
}
