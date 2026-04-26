/* Project 5 Test: Decision Controls and Logical Operations */

int main() {
    int x;
    int y;
    int result;

    /* ── TEST 1: if-then ──────────────────────────────────── */
    print(111);
    x = 5;
    if (x > 3) {
        print(1);
    }

    /* ── TEST 2: if-then-else ────────────────────────────── */
    print(222);
    x = 2;
    if (x > 5) {
        print(0);
    } else {
        print(1);
    }

    /* ── TEST 3: nested if ───────────────────────────────── */
    print(333);
    x = 7;
    y = 3;
    if (x > 5) {
        if (y < 5) {
            print(1);
        } else {
            print(0);
        }
    } else {
        print(0);
    }

    /* ── TEST 4: switch statement ────────────────────────── */
    print(444);
    x = 2;
    switch (x) {
        case 1:
            print(10);
            break;
        case 2:
            print(20);
            break;
        case 3:
            print(30);
            break;
        default:
            print(99);
            break;
    }

    /* ── TEST 5: switch with default hit ─────────────────── */
    print(555);
    x = 9;
    switch (x) {
        case 1:
            print(10);
            break;
        case 2:
            print(20);
            break;
        default:
            print(99);
            break;
    }

    /* ── TEST 6: logical AND (&&) ────────────────────────── */
    print(666);
    x = 5;
    y = 3;
    if (x > 3 && y < 5) {
        print(1);
    } else {
        print(0);
    }

    /* ── TEST 7: logical OR (||) ─────────────────────────── */
    print(777);
    x = 1;
    y = 10;
    if (x > 5 || y > 5) {
        print(1);
    } else {
        print(0);
    }

    /* ── TEST 8: logical NOT (!) ─────────────────────────── */
    print(888);
    x = 0;
    if (!x) {
        print(1);
    } else {
        print(0);
    }

    /* ── TEST 9: combined logical ops ────────────────────── */
    print(999);
    x = 4;
    y = 6;
    result = x > 2 && y > 4;
    print(result);

    return 0;
}
