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

tomasulo_simulator/
├── build/
├── src/
│   ├── decoder.cpp
│   ├── instruction.cpp
│   ├── instruction.h
│   ├── loader.cpp
│   ├── main.cpp
│   ├── tomasulo_sim.cpp
│   ├── tomasulo_sim.h
│   └── translator.cpp
├── tests/
│   ├── bin/ 
│   ├── src/ 
│   ├── build.sh
│   ├── link.ld 
│   ├── generator.sh 
│   └── makefile 
└── README.md

> **Directory and file descriptions:**
> - `build/`: Compiled binaries and object files  
> - `src/`: Source code of the simulator  
>   - `translator.cpp`: Standalone disassembler that converts `.bin` to human-readable RISC-V assembly  
> - `tests/`: Test suite and generation scripts  
>   - `bin/`: Generated binary outputs (`.bin`, `.dis`)  
>   - `src/`: Auto-generated C test programs  
>   - `build.sh`: Compiles C files to raw binary using RISC-V GCC toolchain  
>   - `generator.sh`: Generates synthetic C test cases  
> - `README.md`: This documentation file


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
