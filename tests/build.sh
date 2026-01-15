#!/bin/bash

# Build all C test cases in src/ into:
#   - .bin : raw binary instruction stream (for your simulator)
#   - .dis : human-readable disassembly (for debugging)
#
# Usage:
#   ./build.sh                    # batch build all .c in src/
#   ./build.sh path/to/file.S     # build single .S file

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
OUT_DIR="$SCRIPT_DIR/bin"
TOOLCHAIN_PREFIX="$SCRIPT_DIR/../../riscv/bin/riscv64-unknown-elf"

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
    . = 0x0;
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
EOF

# ==============================
# 处理单个 .S 文件（如果提供）
# ==============================
if [ "$#" -eq 1 ] && [[ "$1" == *.S ]] && [ -f "$1" ]; then
    SFILE="$1"
    BASENAME=$(basename "$SFILE" .S)
    DIRNAME=$(dirname "$SFILE")
    
    OBJ_FILE="$DIRNAME/${BASENAME}.o"
    ELF_FILE="$DIRNAME/${BASENAME}.elf"
    BIN_FILE="$DIRNAME/${BASENAME}.bin"
    DIS_FILE="$DIRNAME/${BASENAME}.dis"

    echo "Building single assembly file: $SFILE"

    # Assemble
    "${TOOLCHAIN_PREFIX}-gcc" \
        -c "$SFILE" \
        -o "$OBJ_FILE" \
        -O0 \
        -march=rv64g \
        -mabi=lp64d \
        -fno-asynchronous-unwind-tables \
        -fno-stack-protector \
        -nostdlib

    # Link
    "${TOOLCHAIN_PREFIX}-ld" \
        -T "$LINKER_SCRIPT" \
        "$OBJ_FILE" -o "$ELF_FILE"

    # Generate outputs
    "${TOOLCHAIN_PREFIX}-objcopy" -O binary --only-section=.text "$ELF_FILE" "$BIN_FILE"
    "${TOOLCHAIN_PREFIX}-objdump" -d -M numeric "$ELF_FILE" > "$DIS_FILE"

    # Clean intermediate files
    rm -f "$OBJ_FILE" "$ELF_FILE"

    echo "Done. Generated:"
    echo "  $BIN_FILE"
    echo "  $DIS_FILE"
    exit 0
fi

# ==============================
# 批量处理 src/*.c
# ==============================
if [ ! -d "$SRC_DIR" ]; then
    echo "Error: Source directory not found: $SRC_DIR"
    echo "Please run this script from the 'tests/' directory."
    exit 1
fi

mkdir -p "$OUT_DIR"
echo "Building Tomasulo test binaries and disassembly..."

shopt -s nullglob
found_any=false
for cfile in "$SRC_DIR"/*.c; do
    found_any=true
    base=$(basename "$cfile" .c)
    obj_file="$OUT_DIR/$base.o"
    elf_file="$OUT_DIR/$base.elf"
    bin_file="$OUT_DIR/$base.bin"
    dis_file="$OUT_DIR/$base.dis"

    echo "  Processing $base.c"

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

    "${TOOLCHAIN_PREFIX}-objcopy" -O binary --only-section=.text "$elf_file" "$bin_file"
    "${TOOLCHAIN_PREFIX}-objdump" -d "$elf_file" > "$dis_file"

    rm -f "$obj_file" "$elf_file"
done

if [ "$found_any" = false ]; then
    echo "No .c files found in $SRC_DIR"
fi

echo
echo "   Done. Outputs in $OUT_DIR/:"
echo "   .bin  → raw instruction bytes (little-endian, 32-bit words)"
echo "   .dis  → disassembled instructions (human readable)"
echo
ls -l "$OUT_DIR"/*.bin "$OUT_DIR"/*.dis 2>/dev/null || echo "No output files."