/* Semantic Error Detection Test
 * Contains only SEMANTIC errors (syntactically correct)
 * Tests compiler's ability to detect and report semantic issues
 */

/* GLOBAL VARIABLES */
int globalA;
int globalB;
int globalC;
int globalC;  /* ERROR 1: Duplicate global variable */

/* GLOBAL ARRAYS */
int validArray[10];
int zeroArray[0];  /* ERROR 2: Zero-size array */

/* FUNCTION DEFINITIONS */
int testFunc(int a, int b) {
    int x;
    int y;
    int x;  /* ERROR 3: Duplicate local variable */

    x = a + b;
    notDeclared = 10;  /* ERROR 4: Undeclared variable */

    return x;
}

int square(int n) {
    int result;
    result = n * n;
    return result;
}

int square(int num) {  /* ERROR 5: Duplicate function definition */
    return num * num;
}

int main() {
    int arr[5];
    int val;

    /* ERROR 6: Using undeclared array */
    unknownArray[0] = 10;

    /* ERROR 7: Using undeclared variable */
    val = mystery + 5;

    /* ERROR 8: Wrong number of arguments */
    val = testFunc(1);  /* Expects 2, got 1 */

    /* ERROR 9: Wrong number of arguments */
    val = testFunc(1, 2, 3);  /* Expects 2, got 3 */

    /* ERROR 10: Calling undeclared function */
    val = unknownFunc(10);

    /* ERROR 11: Assignment to undeclared variable */
    undeclaredVar = 100;

    /* Valid code */
    arr[0] = 42;
    val = square(5);
    print(val);

    return 0;
}

int anotherFunc() {
    return 10;  /* Valid - return inside function */
}

/* ERROR 12: Return outside function */
