// tests/src/raw_int.c
long x2 = 10;
long x3 = 20;
long x5 = 5;
long x1;
long x4;

void _start(void) {
    x1 = x2 + x3;   // RAW: x1 produced
    x4 = x1 + x5;   // x1 consumed
}
