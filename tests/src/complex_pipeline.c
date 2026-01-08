// tests/src/complex_pipeline.c

// Global data (in .data section, 8-byte aligned for doubles)
long g_int_a = 10;
long g_int_b = 20;
long g_int_c = 30;
long g_int_d = 40;

double g_fp_x = 100.0;
double g_fp_y = 5.0;
double g_fp_z = 2.0;

// Results
long r1, r2, r3;
double f1, f2, f3;

void _start(void) {
    // --- Integer Chain (short latency) ---
    r1 = g_int_a + g_int_b;        // ADD  → r1
    r2 = r1 * g_int_c;             // MUL  → r2 (depends on r1)
    r3 = r2 + g_int_d;             // ADD  → r3 (depends on r2)

    // --- Floating-Point Long-Latency Chain ---
    f1 = g_fp_x / g_fp_y;          // FDIV.D → f1 (e.g., 10 cycles)
    f2 = f1 * g_fp_z;              // FMUL.D → f2 (depends on f1)

    // --- Independent Short FP Operation (should run in parallel with f1/f2) ---
    f3 = g_fp_y + g_fp_z;          // FADD.D → f3 (independent!)

    // --- Final integer use of FP result (via bitcast or store/load pattern) ---
    // Note: We don't actually convert; just ensure all instructions are emitted.
    // The simulator only cares about instruction stream and register usage.
}
