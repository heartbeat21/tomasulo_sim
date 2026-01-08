// tests/src/multicycle_mul.c
long a = 7;
long b = 8;
long c = 100;
long d = 200;
long result_mul;
long result_add;

void _start(void) {
    result_mul = a * b;     // MUL (multi-cycle)
    result_add = c + d;     // ADD (single-cycle, should not wait)
}
