// tests/src/load_use.c
long val = 1234;
long tmp;
long result;

void _start(void) {
    tmp = val;          // LD
    result = tmp + 0;   // Use immediately after load
}
