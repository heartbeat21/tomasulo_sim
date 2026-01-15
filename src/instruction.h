// src/instruction.h
#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <cstdint>
#include <string>

enum class OpType {
    ADD, SUB, AND, OR, XOR, SLT, SLTU,
    ADDI, ANDI, ORI, XORI, SLTI, SLTIU,
    SLL, SRL, SRA,
    MUL, MULH, MULHSU, MULHU,
    DIV, DIVU, REM, REMU,
    FADD_D, FSUB_D, FMUL_D, FDIV_D,
    FEQ_D, FLT_D, FLE_D,
    FCVT_D_W, FCVT_W_D,
    LD, SD, LW, SW, FLD, FSD,
    LUI, AUIPC,
    JALR, BNE, EBREAK, UNKNOWN
};

struct Instruction {
    uint32_t raw;
    OpType op;
    int rd = -1;    // integer dest
    int rs1 = -1;   // integer src1
    int rs2 = -1;   // integer src2
    int fd = -1;    // float dest
    int fs1 = -1;   // float src1
    int fs2 = -1;   // float src2
    int32_t imm = 0;
    bool is_fp = false;

    std::string toString() const;

private:
    std::string format(const std::string& tmpl) const;
    static const char* get_op_name(OpType op);
};

#endif