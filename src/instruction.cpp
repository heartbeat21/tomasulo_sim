// src/instruction.cpp
#include "instruction.h"
#include <unordered_map>
#include <functional>
#include <string>

// 寄存器名称表
const char* reg_name_int(int r) {
    static const char* names[] = {
        "x0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };
    if (r >= 0 && r < 32) return names[r];
    return "x?";
}

// 浮点寄存器名称（f0–f31）
std::string reg_name_fp(int f) {
    if (f >= 0 && f < 32) {
        return "f" + std::to_string(f);
    }
    return "f?";
}

// 格式化函数类型
using FormatFn = std::function<std::string(const Instruction&)>;

static const std::unordered_map<OpType, FormatFn>& get_formatters() {
    static const std::unordered_map<OpType, FormatFn> formatters = {
        // 整数 R-type
        {OpType::ADD,   [](const Instruction& i) { 
            return "add " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::SUB,   [](const Instruction& i) { 
            return "sub " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::AND,   [](const Instruction& i) { 
            return "and " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::OR,    [](const Instruction& i) { 
            return "or " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::XOR,   [](const Instruction& i) { 
            return "xor " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::SLT,   [](const Instruction& i) { 
            return "slt " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::SLTU,  [](const Instruction& i) { 
            return "sltu " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::SLL,   [](const Instruction& i) { 
            return "sll " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::SRL,   [](const Instruction& i) { 
            return "srl " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::SRA,   [](const Instruction& i) { 
            return "sra " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},

        // 乘除
        {OpType::MUL,    [](const Instruction& i) { 
            return "mul " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::MULH,   [](const Instruction& i) { 
            return "mulh " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::MULHSU, [](const Instruction& i) { 
            return "mulhsu " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::MULHU,  [](const Instruction& i) { 
            return "mulhu " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::DIV,    [](const Instruction& i) { 
            return "div " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::DIVU,   [](const Instruction& i) { 
            return "divu " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::REM,    [](const Instruction& i) { 
            return "rem " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},
        {OpType::REMU,   [](const Instruction& i) { 
            return "remu " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + reg_name_int(i.rs2); 
        }},

        // 立即数
        {OpType::ADDI,  [](const Instruction& i) { 
            return "addi " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + std::to_string(i.imm); 
        }},
        {OpType::ANDI,  [](const Instruction& i) { 
            return "andi " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + std::to_string(i.imm); 
        }},
        {OpType::ORI,   [](const Instruction& i) { 
            return "ori " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + std::to_string(i.imm); 
        }},
        {OpType::XORI,  [](const Instruction& i) { 
            return "xori " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + std::to_string(i.imm); 
        }},
        {OpType::SLTI,  [](const Instruction& i) { 
            return "slti " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + std::to_string(i.imm); 
        }},
        {OpType::SLTIU, [](const Instruction& i) { 
            return "sltiu " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + std::to_string(i.imm); 
        }},

        // 浮点
        {OpType::FADD_D, [](const Instruction& i) { 
            return "fadd.d " + reg_name_fp(i.fd) + ", " + reg_name_fp(i.fs1) + ", " + reg_name_fp(i.fs2); 
        }},
        {OpType::FSUB_D, [](const Instruction& i) { 
            return "fsub.d " + reg_name_fp(i.fd) + ", " + reg_name_fp(i.fs1) + ", " + reg_name_fp(i.fs2); 
        }},
        {OpType::FMUL_D, [](const Instruction& i) { 
            return "fmul.d " + reg_name_fp(i.fd) + ", " + reg_name_fp(i.fs1) + ", " + reg_name_fp(i.fs2); 
        }},
        {OpType::FDIV_D, [](const Instruction& i) { 
            return "fdiv.d " + reg_name_fp(i.fd) + ", " + reg_name_fp(i.fs1) + ", " + reg_name_fp(i.fs2); 
        }},
        {OpType::FEQ_D,  [](const Instruction& i) { 
            return "feq.d " + std::string(reg_name_int(i.rd)) + ", " + reg_name_fp(i.fs1) + ", " + reg_name_fp(i.fs2); 
        }},
        {OpType::FLT_D,  [](const Instruction& i) { 
            return "flt.d " + std::string(reg_name_int(i.rd)) + ", " + reg_name_fp(i.fs1) + ", " + reg_name_fp(i.fs2); 
        }},
        {OpType::FLE_D,  [](const Instruction& i) { 
            return "fle.d " + std::string(reg_name_int(i.rd)) + ", " + reg_name_fp(i.fs1) + ", " + reg_name_fp(i.fs2); 
        }},

        // 转换（仅支持 W ↔ D，与 instruction.h 一致）
        {OpType::FCVT_D_W, [](const Instruction& i) { 
            return "fcvt.d.w " + reg_name_fp(i.fd) + ", " + reg_name_int(i.rs1); 
        }},
        {OpType::FCVT_W_D, [](const Instruction& i) { 
            return "fcvt.w.d " + std::string(reg_name_int(i.rd)) + ", " + reg_name_fp(i.fs1); 
        }},

        // 内存
        {OpType::LD,  [](const Instruction& i) { 
            return "ld " + std::string(reg_name_int(i.rd)) + ", " + std::to_string(i.imm) + "(" + reg_name_int(i.rs1) + ")"; 
        }},
        {OpType::SD,  [](const Instruction& i) { 
            return "sd " + std::string(reg_name_int(i.rs2)) + ", " + std::to_string(i.imm) + "(" + reg_name_int(i.rs1) + ")"; 
        }},
        {OpType::LW,  [](const Instruction& i) { 
            return "lw " + std::string(reg_name_int(i.rd)) + ", " + std::to_string(i.imm) + "(" + reg_name_int(i.rs1) + ")"; 
        }},
        {OpType::SW,  [](const Instruction& i) { 
            return "sw " + std::string(reg_name_int(i.rs2)) + ", " + std::to_string(i.imm) + "(" + reg_name_int(i.rs1) + ")"; 
        }},
        {OpType::FLD, [](const Instruction& i) { 
            return "fld " + reg_name_fp(i.fd) + ", " + std::to_string(i.imm) + "(" + reg_name_int(i.rs1) + ")"; 
        }},
        {OpType::FSD, [](const Instruction& i) { 
            return "fsd " + reg_name_fp(i.fs2) + ", " + std::to_string(i.imm) + "(" + reg_name_int(i.rs1) + ")"; 
        }},

        // 其他
        {OpType::LUI,   [](const Instruction& i) { 
            return "lui " + std::string(reg_name_int(i.rd)) + ", " + std::to_string(i.imm >> 12); 
        }},
        {OpType::AUIPC, [](const Instruction& i) { 
            return "auipc " + std::string(reg_name_int(i.rd)) + ", " + std::to_string(i.imm >> 12); 
        }},
        {OpType::EBREAK,[](const Instruction& i) { 
            (void)i;
            return "ebreak"; 
        }},
        {OpType::JALR, [](const Instruction& i) { 
            return "jalr " + std::string(reg_name_int(i.rd)) + ", " + reg_name_int(i.rs1) + ", " + std::to_string(i.imm); 
        }},
        {OpType::BNE, [](const Instruction& i) { 
            return "bne " + std::string(reg_name_int(i.rs1)) + ", " + reg_name_int(i.rs2) + ", " + std::to_string(i.imm); 
        }},
    };
    return formatters;
}

std::string Instruction::toString() const {
    const auto& formatters = get_formatters();
    auto it = formatters.find(op);
    if (it != formatters.end()) {
        return it->second(*this);
    } else {
        return "unknown (raw=0x" + std::to_string(raw) + ")";
    }
}