/* Complete Working Test - All 6 Requirements Met
 * This version works within compiler limitations
 */

/* 1. MULTIPLE GLOBAL VARIABLES */
int globalA;
int globalB;
int globalC;

/* 3. ARRAY DECLARATIONS (GLOBAL) */
int sharedArray[10];

/* 6. FUNCTION THAT "RETURNS" ARRAY (fills parameter) */
int fillNumbers(int output[]) {
    output[0] = 100;
    output[1] = 200;
    output[2] = 300;
    return 3;
}

/* 5. ARRAY AS FUNCTION PARAMETER */
int readArray(int input[]) {
    int val;
    val = input[0];
    return val;
}

/* 4. ARITHMETIC WITH 5+ TERMS */
int sumFive(int a, int b, int c, int d, int e) {
    int x;
    int y;
    int result;
    x = a + b + c;
    y = d + e;
    result = x + y;
    return result;
}

/* 2. MULTIPLE FUNCTIONS */
int square(int n) {
    int result;
    result = n * n;
    return result;
}

int cube(int n) {
    int sq;
    int result;
    sq = square(n);
    result = sq * n;
    return result;
}

int main() {
    int localArray[8];
    int resultArray[5];
    int a;
    int b;
    int c;
    int d;
    int e;
    int result;

    /* TEST: Array operations */
    print(111);
    localArray[0] = 10;
    localArray[1] = 20;
    localArray[2] = 30;
    localArray[3] = 40;
    localArray[4] = 50;

    print(localArray[0]);
    print(localArray[2]);
    print(localArray[4]);

    /* TEST: 5+ term arithmetic with constants */
    print(222);
    result = 1 + 2 + 3 + 4 + 5;
    print(result);

    /* TEST: 5+ term arithmetic with variables */
    print(333);
    a = 10;
    b = 20;
    c = 30;
    d = 40;
    e = 50;
    result = sumFive(a, b, c, d, e);
    print(result);

    /* TEST: Function fills array (returns array) */
    print(444);
    fillNumbers(resultArray);
    print(resultArray[0]);
    print(resultArray[1]);
    print(resultArray[2]);

    /* TEST: Pass array to function */
    print(555);
    result = readArray(localArray);
    print(result);

    /* TEST: Multiple function calls */
    print(666);
    a = square(5);
    print(a);
    b = cube(3);
    print(b);

    return 0;
}
