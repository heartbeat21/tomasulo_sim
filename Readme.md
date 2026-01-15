# TomasuloSim

## Project Overview
This project implements a Tomasulo algorithm-based simulator for the RISC-V instruction set architecture (RV32I + RV32F), supporting integer and floating-point arithmetic, memory operations, and basic control flow. It faithfully models key microarchitectural features of modern out-of-order processors, including register renaming, reservation stations, reorder buffer (ROB), load-store queue (LSQ), and a common data bus (CDB) to enable out-of-order execution with in-order commit.
The simulator is designed for educational purposes, allowing users to explore how modern CPUs handle instruction-level parallelism, dependencies, and pipeline hazards.

## Key Features
- Full support for RISC-V RV32I base integer instruction set (e.g., ADD, SUB, MUL, DIV, LD, SD)
- Support for RISC-V RV32F single-precision floating-point instructions (e.g., FADD.S, FMUL.S, FDIV.S)
- Accurate modeling of memory operations (LW, SW, FLW, FSW) with dependency tracking via LSQ
- Complete Tomasulo-with-ROB pipeline:
  - Reservation Stations (RS) for ALU, Integer Multiply/Divide, Load, Store, and FP units
  - Functional Units with configurable latencies (e.g., FP divide = 10 cycles)
  - Reorder Buffer (ROB) with 32 entries ensuring precise exceptions and program-order commit
  - Load-Store Queue (LSQ) for memory disambiguation and out-of-order memory access
  - Common Data Bus (CDB) for result broadcasting
- Exception handling (e.g., division by zero, illegal instruction)
- Cycle-accurate simulation with detailed per-cycle state dump for debugging

## Project Structure

```
tomasulo_simulator/
├── build/                  # Compiled binaries and object files
├── src/
│   ├── decoder.cpp         # Instruction decoder (used by simulator)
│   ├── instruction.cpp     # Instruction class implementation
│   ├── instruction.h       # Instruction enums and definitions
│   ├── loader.cpp          # Binary (.bin) file loader
│   ├── main.cpp            # Simulator entry point
│   ├── tomasulo_sim.cpp    # Core Tomasulo algorithm logic
│   ├── tomasulo_sim.h      # TomasuloSim class declaration
│   └── translator.cpp      # Standalone disassembler: .bin → human-readable RISC-V asm
├── tests/
│   ├── bin/                # Generated outputs: .bin (raw code), .dis (GCC disasm)
│   ├── src/                # Source files for test cases (restricted C)
│   ├── build.sh            # Compiles .c → .bin/.dis using RISC-V GCC toolchain
│   ├── link.ld             # Linker script (code starts at 0x0)
│   ├── generator.sh        # Generates test .c files
│   └── makefile            # Build system for simulator
└── README.md               # This documentation file
```

> Note:
> - `build.sh` uses the official RISC-V GCC toolchain to produce correct .bin files.
> - `translator.cpp` is a custom disassembler that helps you inspect the contents of .bin files without relying on `objdump`. It is useful for verifying instruction encoding or debugging the loader.

## Building and Running

### 1. Compile the Simulator

``` bash
make clean
make
```

> Use `make debug` for a debug build with full symbols and no optimizations.

### 2. Generate and Build Test Programs

``` bash
# Step 1: Generate C test cases
./generator.sh

# Step 2: Compile them to .bin (for simulator) and .dis (for reference)
cd tests
./build.sh
```

The `build.sh` script:
- Uses `riscv64-unknown-elf-gcc` and `ld` to compile and link each `.c` file
- Places code at address `0x0` via `link.ld`
- Extracts the `.text` section as a raw binary (`*.bin`)
- Generates an official disassembly (`*.dis`) using `riscv64-unknown-elf-objdump`

### 3. (Optional) Use Your Custom Disassembler

You can also use the built-in `translator` to disassemble any `.bin` file:

``` bash
./build/translator tests/bin/raw_int.bin
```

This will print a simple, human-readable listing of the RISC-V instructions contained in the binary—useful for quick validation or when external tools aren’t available.

### 4. Run the Tomasulo Simulator

``` bash
./build/tomasulo tests/bin/raw_int.bin
```

The simulator loads the raw instruction stream and executes it cycle-by-cycle using the Tomasulo algorithm, printing detailed pipeline state at each step.

## Limitations

- **No branch prediction**: All branches are assumed not taken; misprediction recovery not modeled.
- **No cache/memory hierarchy**: Memory is a flat byte-addressable space with uniform latency.
- **Single-issue pipeline**: Only one instruction is issued per cycle.
- **No interrupts or system calls**: Pure user-mode execution.
- **Limited C support in tests**: `generator.sh` produces straight-line code only (no loops, conditionals).

## Addition

> add for homework

Since the assignment requires simulating a loop, I have added the `bne` instruction for loop control. The additional test case is located in `addition_test/simple_test.S`.

The loop is as follows:
``` MIPS
LOOP:
    L.D        F0,0(R1)
    MUL.D     F4,F0,F2
    S.D        0(R1),F4
    DADDUI    R1,R1,#-8
    BNE        R1,R2,LOOP
```

To adapt it to my simulator, I rewrote it using RISC-V assembly instructions (and modified `tests/build.sh`， to support compiling individual `.S` files)
``` RISCV
LOOP:
    fld     f0, 0(t0)
    fmul.d  f4, f0, f2
    fsd     f4, 0(t0)
    addi    t0, t0, -8
    bne     t0, t1, LOOP
```

### 1. Handling of BNE

- Decoding：`BNE` is a B-type instruction; immediate sign-extension is a bit tricky but relatively straightforward to resolve.
- Execution:`BNE` still uses the `INTALU` functional unit, so no additional `Function Units` need to be added.
- PC update: Thanks to RISC-V’s branch-without-delay-slot design, the computed target PC can be used directly in the next cycle, avoiding stalls due to the lack of branch prediction during issue.

> *!NOTE*
> There is an important detail here: the correct calculation is `next_pc = pc + imm`，not `next_pc = (pc + 4) + imm`.

### 2. Using ROB Values

I previously made an error: when a new instruction was in the ISSUE stage, I did not fetch available operand values from the ROB entry, but instead read directly from the register status table. This incorrectly set `Qj` rather than properly using `Vj`.

To fix this bug, I had to introduce the following conditional logic:
``` C++
if (regs_int_status[data_reg.idx] == "") {
  rs.Vk = OperandValue(regs_int[data_reg.idx]);
} else {
  // here
  int dep_rob_idx = get_rob_index(regs_int_status[data_reg.idx]);
  if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
      rs.Vk = rob[dep_rob_idx].result;
  } else {
      rs.Qk = regs_int_status[data_reg.idx];
  }
}
```

However, such conditional checks would be inefficient in actual hardware. I am not sure if there is a better approach.

### 3. Memory and Register Initialization

In the original version, everything was initialized to empty or zero. Now, configurable initialization has been added.

For example, the above loop test uses the following initialization:
```C++
std::vector<double> input_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
  uint64_t base_addr = 0x1000;
  uint64_t start_addr = base_addr + (input_data.size() - 1) * sizeof(double); // 0x1040
  uint64_t end_addr = base_addr; // 0x1000

  // 构建内存初始化数据
  MemoryInitData mem_init;
  for (size_t i = 0; i < input_data.size(); ++i) {
      uint64_t addr = base_addr + i * sizeof(double);
      mem_init.fp_data.push_back({addr, input_data[i]});
  }
```

### 4. Results

Final cycle and memory state (`t0` is an alias for `x5`， similarly `t1` corresponds to `x6`)
```
========== CYCLE 40 ==========

Integer Register Status:
FP Register Status:

Integer Register value:
  x5 <- 4096      x6 <- 4096
FP Register value:
  f0 <- 2         f2 <- 2         f4 <- 4
========================================

 {addr : val}
===================  memory int data =====================

===================  memory fp data =====================
{ 4152 : 16 }   { 4144 : 14 }   { 4136 : 12 }   { 4128 : 10 }   { 4120 : 8 }    { 4112 : 6 }    { 4104 : 4 }    { 4096 : 1 }
```

This matches the expected behavior of processing from high to low addresses (since `R2=base_addr`，the value at address `4096` was not multiplied).

### 5. Result Analysis

```C++
int get_latency(OpType op) {
  if (is_alu_op(op)) return 1;
  if (is_muldiv_op(op)) return 3;
  if (is_load_op(op)) return 2;
  if (is_store_op(op)) return 1;
  if (is_fp_add_op(op)) return 2;
  if (op == OpType::FMUL_D || op == OpType::FCVT_D_W || op == OpType::FCVT_W_D) return 4;
  if (op == OpType::FDIV_D) return 8;
  return 1;
}
```

The latencies are configured as above.

Counting from `cycle 0`:

- `fld` should complete execution in `1 + 2 = 3` cycles and commit in `cycle 3`. `fmul`also starts execution in this cycle.
- `fmul` requires 4 cycles and should commit in `cycle 7`.
- `fsd` should commit in `cycle 9`.
- Over seven loop iterations, other instruction latencies are overlapped; the main bottleneck comes from `fmul` and `fsd`.
- The total cycle count is `1 + 2 + (4 + 1) * 7 + 3 + 1 = 41`. The simulation result is correct.

