// tests/src/fp_mul_div.c
double x = 10.0;
double y = 2.0;
double z = 4.0;
double temp;
double result;

void _start(void) {
    temp = x * y;       // FMUL.D (e.g., 4 cycles)
    result = temp / z;  // FDIV.D (e.g., 10 cycles, depends on temp)
}
