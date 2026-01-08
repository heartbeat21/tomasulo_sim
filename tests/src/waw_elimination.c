// tests/src/waw_elimination.c
long slow_input1 = 100;
long slow_input2 = 200;
long fast_input1 = 1;
long fast_input2 = 2;
long result;

void _start(void) {
    result = slow_input1 * slow_input2;  // Slow: MUL (writes result)
    result = fast_input1 + fast_input2;  // Fast: ADD (overwrites result)
    // Final value must be 3, not 20000
}
