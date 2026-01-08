#!/bin/bash

# Build all C test cases in src/ into:
#   - .bin : raw binary instruction stream (for your simulator)
#   - .dis : human-readable disassembly (for debugging)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
OUT_DIR="$SCRIPT_DIR/bin"
TOOLCHAIN_PREFIX="$SCRIPT_DIR/../../riscv/bin/riscv64-unknown-elf"

if [ ! -d "$SRC_DIR" ]; then
    echo "Error: Source directory not found: $SRC_DIR"
    echo "Please run this script from the 'tests/' directory."
    exit 1
fi

if [ ! -f "${TOOLCHAIN_PREFIX}-gcc" ]; then
    echo "Error: RISC-V GCC not found at: ${TOOLCHAIN_PREFIX}-gcc"
    echo "Expected toolchain path: $SCRIPT_DIR/../riscv/"
    exit 1
fi

LINKER_SCRIPT="$SCRIPT_DIR/link.ld"
cat > "$LINKER_SCRIPT" <<EOF
ENTRY(_start)
SECTIONS
{
    . = 0x0;          /* 代码从地址 0 开始 */
    .text : { *(.text) }
    .data : { *(.data) }  /* 数据紧跟代码后 */
    .bss : { *(.bss) }
}
EOF

mkdir -p "$OUT_DIR"

echo "Building Tomasulo test binaries and disassembly..."

for cfile in "$SRC_DIR"/*.c; do
    if [ ! -e "$cfile" ]; then
        echo "No .c files found in $SRC_DIR"
        exit 0
    fi

    base=$(basename "$cfile" .c)
    obj_file="$OUT_DIR/$base.o"
    elf_file="$OUT_DIR/$base.elf"
    bin_file="$OUT_DIR/$base.bin"
    dis_file="$OUT_DIR/$base.dis"

    echo "  Processing $base.c"

    # Compile to object file
    "${TOOLCHAIN_PREFIX}-gcc" \
        -c "$cfile" \
        -o "$obj_file" \
        -O0 \
        -march=rv64g \
        -mabi=lp64d \
        -fno-asynchronous-unwind-tables \
        -fno-stack-protector \
        -nostdlib

    "${TOOLCHAIN_PREFIX}-ld" \
        -T "$LINKER_SCRIPT" \
        "$obj_file" -o "$elf_file"

    # Generate raw binary (for simulator)
    "${TOOLCHAIN_PREFIX}-objcopy" -O binary --only-section=.text "$elf_file" "$bin_file"

    # Generate human-readable disassembly
    "${TOOLCHAIN_PREFIX}-objdump" -d "$elf_file" > "$dis_file"

    # Optional: also keep the assembly source (from GCC -S)
    # "${TOOLCHAIN_PREFIX}-gcc" -S -O0 -march=rv64g -mabi=lp64d "$cfile" -o "$OUT_DIR/$base.s"
    rm $obj_file $elf_file
done

echo
echo "   Done. Outputs in $OUT_DIR/:"
echo "   .bin  → raw instruction bytes (little-endian, 32-bit words)"
echo "   .dis  → disassembled instructions (human readable)"
echo
ls -l "$OUT_DIR"/*.bin "$OUT_DIR"/*.dis