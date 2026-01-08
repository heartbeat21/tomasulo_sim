// src/decoder.cpp
#include "instruction.h"
#include <cstdint>
#include <cstdio>

static inline uint32_t get_opcode(uint32_t inst) { return inst & 0x7F; }
static inline uint32_t get_rd(uint32_t inst)     { return (inst >> 7) & 0x1F; }
static inline uint32_t get_funct3(uint32_t inst) { return (inst >> 12) & 0x7; }
static inline uint32_t get_rs1(uint32_t inst)    { return (inst >> 15) & 0x1F; }
static inline uint32_t get_rs2(uint32_t inst)    { return (inst >> 20) & 0x1F; }
static inline uint32_t get_funct7(uint32_t inst) { return (inst >> 25) & 0x7F; }

static int32_t decode_imm_i(uint32_t inst) {
    return static_cast<int32_t>(inst) >> 20;
}

static int32_t decode_imm_s(uint32_t inst) {
    int32_t imm11_5 = (inst >> 25) & 0x7F;
    int32_t imm4_0  = (inst >> 7)  & 0x1F;
    int32_t imm = (imm11_5 << 5) | imm4_0;
    if (imm & (1 << 11)) imm |= 0xFFFFF000; // sign-extend to 32-bit
    return imm;
}

Instruction decode_instruction(uint32_t inst_word) {
    Instruction inst{};
    inst.raw = inst_word;

    const uint32_t opcode = get_opcode(inst_word);
    const uint32_t OP_LOAD   = 0x03;
    const uint32_t OP_STORE  = 0x23;
    const uint32_t OP_OP     = 0x33;
    const uint32_t OP_FLOAD  = 0x07;
    const uint32_t OP_FSTORE = 0x27;
    const uint32_t OP_FP     = 0x53;
    const uint32_t OP_IMM    = 0x13;
    const uint32_t OP_LUI    = 0x37;
    const uint32_t OP_AUIPC  = 0x17;
    const uint32_t OP_JALR   = 0x67;
    const uint32_t OP_MISC_MEM = 0x73;

    if (opcode == OP_LOAD) {
        uint32_t funct3 = get_funct3(inst_word);
        inst.rd = get_rd(inst_word);
        inst.rs1 = get_rs1(inst_word);
        inst.imm = decode_imm_i(inst_word);
        if (funct3 == 0x3) {
            inst.op = OpType::LD;
        } else if (funct3 == 0x2) {
            inst.op = OpType::LW;
        } else {
            inst.op = OpType::UNKNOWN;
        }
    }
    else if (opcode == OP_STORE) {
        uint32_t funct3 = get_funct3(inst_word);
        inst.rs1 = get_rs1(inst_word);
        inst.rs2 = get_rs2(inst_word);
        inst.imm = decode_imm_s(inst_word);
        if (funct3 == 0x3) {
            inst.op = OpType::SD;
        } else if (funct3 == 0x2) {
            inst.op = OpType::SW;
        } else {
            inst.op = OpType::UNKNOWN;
        }
    }
    else if (opcode == OP_OP) {
        uint32_t f3 = get_funct3(inst_word);
        uint32_t f7 = get_funct7(inst_word);
        inst.rd = get_rd(inst_word);
        inst.rs1 = get_rs1(inst_word);
        inst.rs2 = get_rs2(inst_word);

        if (f3 == 0x0 && f7 == 0x00) inst.op = OpType::ADD;
        else if (f3 == 0x0 && f7 == 0x20) inst.op = OpType::SUB;
        else if (f3 == 0x0 && f7 == 0x01) inst.op = OpType::MUL;
        else if (f3 == 0x4 && f7 == 0x01) inst.op = OpType::DIV;
        else if (f3 == 0x6 && f7 == 0x01) inst.op = OpType::REM;
        else if (f3 == 0x1 && f7 == 0x00) inst.op = OpType::SLL;
        else if (f3 == 0x5 && f7 == 0x00) inst.op = OpType::SRL;
        else if (f3 == 0x5 && f7 == 0x20) inst.op = OpType::SRA;
        else if (f3 == 0x2 && f7 == 0x00) inst.op = OpType::SLT;
        else if (f3 == 0x3 && f7 == 0x00) inst.op = OpType::SLTU;
        else if (f3 == 0x4 && f7 == 0x00) inst.op = OpType::XOR;
        else if (f3 == 0x6 && f7 == 0x00) inst.op = OpType::OR;
        else if (f3 == 0x7 && f7 == 0x00) inst.op = OpType::AND;
        else inst.op = OpType::UNKNOWN;
    }
    else if (opcode == OP_FP) {
        uint32_t f3 = get_funct3(inst_word); // fmt
        uint32_t f7 = get_funct7(inst_word);
        inst.fd = get_rd(inst_word);
        inst.fs1 = get_rs1(inst_word);
        inst.fs2 = get_rs2(inst_word); // actually 'rm' for conversions
        inst.is_fp = true;

        if (f3 == 0x3 || f3 == 0x7) {
            // Arithmetic: fadd.d, fsub.d, etc.
            switch (f7) {
                case 0x01:
                case 0x02: inst.op = OpType::FADD_D; break;
                case 0x05: inst.op = OpType::FSUB_D; break;
                case 0x09: inst.op = OpType::FMUL_D; break;
                case 0x0D: inst.op = OpType::FDIV_D; break;
                default: inst.op = OpType::UNKNOWN;
            }
        }
        else if (f3 == 0 && (f7 == 0x60 || f7 == 0x68 || f7 == 0x69)) {
            // fcvt.d.w  → int32 to double
            inst.rs1 = inst.fs1;
            inst.fs1 = -1;
            inst.op = OpType::FCVT_D_W;
        }
        else if (f3 == 0x1 && (f7 == 0x60 || f7 == 0x61)) {
            // fcvt.w.d  → double to int32(actually the compiler will use fcvt.wu.d)
            inst.rd = inst.fd;
            inst.fd = -1;
            inst.op = OpType::FCVT_W_D;
        }
        else {
            inst.op = OpType::UNKNOWN;
        }
    }
    else if (opcode == OP_FLOAD) {
        uint32_t funct3 = get_funct3(inst_word);
        inst.fd = get_rd(inst_word);
        inst.rs1 = get_rs1(inst_word);
        inst.imm = decode_imm_i(inst_word);
        inst.is_fp = true;
        if (funct3 == 0x3) {
            inst.op = OpType::FLD;
        } else {
            inst.op = OpType::UNKNOWN;
        }
    }
    else if (opcode == OP_FSTORE) {
        uint32_t funct3 = get_funct3(inst_word);
        inst.fs2 = get_rs2(inst_word);
        inst.rs1 = get_rs1(inst_word);
        inst.imm = decode_imm_s(inst_word);
        inst.is_fp = true;
        if (funct3 == 0x3) {
            inst.op = OpType::FSD;
        } else {
            inst.op = OpType::UNKNOWN;
        }
    }
    else if (opcode == OP_IMM) {
        uint32_t funct3 = get_funct3(inst_word);
        inst.rd = get_rd(inst_word);
        inst.rs1 = get_rs1(inst_word);
        inst.imm = decode_imm_i(inst_word);

        if (funct3 == 0x0) inst.op = OpType::ADDI;
        else if (funct3 == 0x7) inst.op = OpType::ANDI;
        else if (funct3 == 0x6) inst.op = OpType::ORI;
        else if (funct3 == 0x4) inst.op = OpType::XORI;
        else if (funct3 == 0x2) inst.op = OpType::SLTI;
        else if (funct3 == 0x3) inst.op = OpType::SLTIU;
        else inst.op = OpType::UNKNOWN;
    }
    else if (opcode == OP_LUI) {
        inst.op = OpType::LUI;
        inst.rd = get_rd(inst_word);
        inst.imm = static_cast<int32_t>(inst_word & 0xFFFFF000);
    }
    else if (opcode == OP_AUIPC) {
        inst.op = OpType::AUIPC;
        inst.rd = get_rd(inst_word);
        inst.imm = static_cast<int32_t>(inst_word & 0xFFFFF000);
    }
    else if (opcode == OP_JALR) {
        inst.op = OpType::JALR;
        inst.rd = get_rd(inst_word);
        inst.rs1 = get_rs1(inst_word);
        inst.imm = decode_imm_i(inst_word);
    }
    else if (opcode == OP_MISC_MEM && (inst_word & 0x000FFFFF) == 0x00000073) {
        if (get_funct3(inst_word) == 0 && get_rs1(inst_word) == 0 && get_rd(inst_word) == 0) {
            inst.op = OpType::EBREAK;
        } else {
            inst.op = OpType::UNKNOWN;
        }
    }
    else {
        inst.op = OpType::UNKNOWN;
    }

    return inst;
}