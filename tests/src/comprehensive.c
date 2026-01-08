// src/comprehensive.c
// Comprehensive test for Tomasulo simulator: covers integer, FP, control flow

// Global variables (will be in .data or .bss)
long long int_a = 100;
long long int_b = 200;
long long int_c;

double fp_a = 1.5;
double fp_b = 2.5;
double fp_c;

void _start(void) {
    // Integer operations
    long long tmp1 = int_a + int_b;   // add
    long long tmp2 = int_a - int_b;   // sub
    long long tmp3 = int_a * int_b;   // mul
    long long tmp4 = int_a / int_b;   // div
    long long tmp5 = int_a % int_b;   // rem

    int_c = tmp1 + tmp2 + tmp3 + tmp4 + tmp5;

    // Floating-point arithmetic
    double f1 = fp_a + fp_b;  // fadd.d
    double f2 = fp_a - fp_b;  // fsub.d
    double f3 = fp_a * fp_b;  // fmul.d
    double f4 = fp_a / fp_b;  // fdiv.d

    fp_c = f1 + f2 + f3 + f4;

    // FP <-> Integer conversion
    int i32 = 42;
    double d32 = (double)i32;      // → fcvt.d.w
    int j32 = (int)d32;            // → fcvt.w.d

    volatile int sink_i = j32;
    volatile double sink_d = d32;

    // Return (generates 'ret' = jalr x0, x1, 0)
    return;
}