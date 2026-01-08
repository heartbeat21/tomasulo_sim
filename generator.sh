#!/bin/bash

# Generate a complete tests/src/ directory with 6 Tomasulo test cases in C

set -e

TESTS_DIR="tests"
SRC_DIR="$TESTS_DIR/src"

mkdir -p "$SRC_DIR"

# 1. raw_int.c
cat > "$SRC_DIR/raw_int.c" << 'EOF'
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
EOF

# 2. multicycle_mul.c
cat > "$SRC_DIR/multicycle_mul.c" << 'EOF'
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
EOF

# 3. load_use.c
cat > "$SRC_DIR/load_use.c" << 'EOF'
// tests/src/load_use.c
long val = 1234;
long tmp;
long result;

void _start(void) {
    tmp = val;          // LD
    result = tmp + 0;   // Use immediately after load
}
EOF

# 4. fp_add.c
cat > "$SRC_DIR/fp_add.c" << 'EOF'
// tests/src/fp_add.c
double a = 1.5;
double b = 2.5;
double c;

void _start(void) {
    c = a + b;          // FADD.D
}
EOF

# 5. fp_mul_div.c
cat > "$SRC_DIR/fp_mul_div.c" << 'EOF'
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
EOF

# 6. waw_elimination.c
cat > "$SRC_DIR/waw_elimination.c" << 'EOF'
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
EOF

# 7. complex_pipeline.c
cat > "$SRC_DIR/complex_pipeline.c" << 'EOF'
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
EOF

echo " Successfully generated:"
ls -1 "$SRC_DIR"