// src/tomasulo_sim.cpp
#include "tomasulo_sim.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>

uint64_t regs_int[32] = {0};
double regs_fp[32] = {0.0};

std::string regs_int_status[32] = {};
std::string regs_fp_status[32] = {};

std::unordered_map<uint64_t, uint64_t> memory_int;
std::unordered_map<uint64_t, double> memory_fp;

ReservationStation intalu_rs[NUM_INTALU_RS];
ReservationStation muldiv_rs[NUM_MULDIV_RS];
ReservationStation load_rs[NUM_LOAD_RS];
ReservationStation store_rs[NUM_STORE_RS];
ReservationStation fpadd_rs[NUM_FPADD_RS];
ReservationStation fpmul_rs[NUM_FPMUL_RS];
ReservationStation fpdiv_rs[NUM_FPDIV_RS];

std::array<FunctionalUnit, NUM_INT_ALUS> int_alu_fus;
std::array<FunctionalUnit, NUM_LOAD_UNITS> load_fus;
std::array<FunctionalUnit, NUM_FP_ADDERS> fp_add_fus;
std::array<FunctionalUnit, NUM_FP_MULTIPLIERS> fp_mul_fus;
std::array<FunctionalUnit, 1> store_fus;
std::array<FunctionalUnit, 1> int_muldiv_fu;
std::array<FunctionalUnit, 1> fp_div_fu;

ROBEntry rob[ROB_SIZE];
int rob_head = 0;
int rob_tail = 0;
int rob_count = 0;

std::vector<CDB> cdb_list;

LSQEntry lsq[LSQ_SIZE];
int lsq_head = 0;
int lsq_tail = 0;
int lsq_count = 0;

std::vector<Instruction> instruction_queue;
size_t next_fetch_idx = 0;
size_t next_fetch_branch = 0;

std::string get_rs_id(const std::string& type, int idx) {
    return type + std::to_string(idx);
}

// 分类函数
bool is_alu_op(OpType op) {
    switch (op) {
        case OpType::ADD: case OpType::SUB: case OpType::AND: case OpType::OR:
        case OpType::XOR: case OpType::SLT: case OpType::SLTU:
        case OpType::ADDI: case OpType::ANDI: case OpType::ORI: case OpType::XORI:
        case OpType::SLTI: case OpType::SLTIU:
        case OpType::SLL: case OpType::SRL: case OpType::SRA:
        case OpType::LUI: case OpType::AUIPC: case OpType::JALR:
        case OpType::BNE:
            return true;
        default: return false;
    }
}

bool is_muldiv_op(OpType op) {
    switch (op) {
        case OpType::MUL: case OpType::MULH: case OpType::MULHSU: case OpType::MULHU:
        case OpType::DIV: case OpType::DIVU: case OpType::REM: case OpType::REMU:
            return true;
        default: return false;
    }
}

bool is_load_op(OpType op) {
    return op == OpType::LD || op == OpType::LW || op == OpType::FLD;
}

bool is_store_op(OpType op) {
    return op == OpType::SD || op == OpType::SW || op == OpType::FSD;
}

bool is_fp_add_op(OpType op) {
    return op == OpType::FADD_D || op == OpType::FSUB_D ||
           op == OpType::FEQ_D || op == OpType::FLT_D || op == OpType::FLE_D;
}

bool is_fp_mul_op(OpType op) {
    return op == OpType::FMUL_D || op == OpType::FCVT_D_W || op == OpType::FCVT_W_D;
}

bool is_fp_div_op(OpType op) {
    return op == OpType::FDIV_D;
}

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

static DestReg get_dest_reg_from_instruction(const Instruction& instr) {
    if (instr.rd >= 0) {
        return IntReg{static_cast<uint8_t>(instr.rd)};
    } else if (instr.fd >= 0) {
        return FpReg{static_cast<uint8_t>(instr.fd)};
    } else {
        return std::monostate{};
    }
}

std::string format_operand_value(const OperandValue& val) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, uint64_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            // 可选：控制浮点精度，避免输出 3.141592653589793115997...
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << v;
            std::string s = oss.str();
            // 移除尾随零和可能的小数点（可选）
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            s.erase(s.find_last_not_of('.') + 1, std::string::npos);
            return s;
        } else {
            return "?";
        }
    }, val);
}

static OperandValue execute_alu_op(const ReservationStation& rs) {
    auto vj = rs.Vj.value();
    auto vk = rs.Vk.value();
    uint64_t j = to_int(vj);
    uint64_t k = to_int(vk);

    switch (rs.op) {
        case OpType::ADD:  return OperandValue(j + k);
        case OpType::SUB:  return OperandValue(j - k);
        case OpType::AND:  return OperandValue(j & k);
        case OpType::OR:   return OperandValue(j | k);
        case OpType::XOR:  return OperandValue(j ^ k);
        case OpType::SLT:  return OperandValue(static_cast<int64_t>(j) < static_cast<int64_t>(k) ? 1ULL : 0ULL);
        case OpType::SLTU: return OperandValue(j < k ? 1ULL : 0ULL);
        case OpType::ADDI: return OperandValue(j + k);
        case OpType::ANDI: return OperandValue(j & k);
        case OpType::ORI:  return OperandValue(j | k);
        case OpType::XORI: return OperandValue(j ^ k);
        case OpType::SLTI: return OperandValue(static_cast<int64_t>(j) < static_cast<int64_t>(k) ? 1ULL : 0ULL);
        case OpType::SLTIU:return OperandValue(j < k ? 1ULL : 0ULL);
        case OpType::SLL:  return OperandValue(j << (k & 0x3F));
        case OpType::SRL:  return OperandValue(j >> (k & 0x3F));
        case OpType::SRA:  return OperandValue(static_cast<uint64_t>(j) >> (k & 0x3F));
        case OpType::LUI:  return vk;
        case OpType::AUIPC: {
            uint64_t pc = static_cast<uint64_t>(rs.A);
            uint64_t uimm = to_int(vk);
            return OperandValue(pc + uimm);
        }
        case OpType::JALR: {
            return OperandValue(static_cast<uint64_t>(rs.A)); // 返回地址 = PC+4
        }
        case OpType::BNE: {
            // BNE: if (rs1 != rs2) then branch (result is 1), else 0.
            return OperandValue((j != k) ? 1ULL : 0ULL);
        }
        default:
            throw std::runtime_error("Unsupported ALU op");
    }
}

static OperandValue execute_muldiv_op(const ReservationStation& rs) {
    auto vj = rs.Vj.value();
    auto vk = rs.Vk.value();
    int64_t j = static_cast<int64_t>(to_int(vj));
    int64_t k = static_cast<int64_t>(to_int(vk));
    uint64_t uj = to_int(vj);
    uint64_t uk = to_int(vk);

    switch (rs.op) {
        case OpType::MUL:    return OperandValue(static_cast<uint64_t>(j * k));
        case OpType::MULH:   return OperandValue(static_cast<uint64_t>((__int128_t)j * k >> 64));
        case OpType::MULHSU: return OperandValue(static_cast<uint64_t>((__int128_t)j * uk >> 64));
        case OpType::MULHU:  return OperandValue(static_cast<uint64_t>((unsigned __int128)uj * uk >> 64));
        case OpType::DIV:
            if (k == 0) return OperandValue(static_cast<uint64_t>(-1));
            return OperandValue(static_cast<uint64_t>(j / k));
        case OpType::DIVU:
            if (uk == 0) return OperandValue(~0ULL);
            return OperandValue(uj / uk);
        case OpType::REM:
            if (k == 0) return vj;
            return OperandValue(static_cast<uint64_t>(j % k));
        case OpType::REMU:
            if (uk == 0) return vj;
            return OperandValue(uj % uk);
        default:
            throw std::runtime_error("Unsupported MUL/DIV op");
    }
}

static OperandValue execute_fp_add_op(const ReservationStation& rs) {
    double fj = to_fp(rs.Vj.value());
    double fk = to_fp(rs.Vk.value());
    switch (rs.op) {
        case OpType::FADD_D: return OperandValue(fj + fk);
        case OpType::FSUB_D: return OperandValue(fj - fk);
        case OpType::FEQ_D:  return OperandValue(fj == fk ? 1.0 : 0.0);
        case OpType::FLT_D:  return OperandValue(fj < fk ? 1.0 : 0.0);
        case OpType::FLE_D:  return OperandValue(fj <= fk ? 1.0 : 0.0);
        default: throw std::runtime_error("Unsupported FP add op");
    }
}

static OperandValue execute_fp_mul_op(const ReservationStation& rs) {
    if (rs.op == OpType::FCVT_D_W) {
        int32_t i = static_cast<int32_t>(to_int(rs.Vj.value()));
        return OperandValue(static_cast<double>(i));
    } else if (rs.op == OpType::FCVT_W_D) {
        double d = to_fp(rs.Vj.value());
        int32_t i = static_cast<int32_t>(d);
        return OperandValue(static_cast<uint64_t>(static_cast<uint32_t>(i)));
    } else if (rs.op == OpType::FMUL_D) {
        double fj = to_fp(rs.Vj.value());
        double fk = to_fp(rs.Vk.value());
        return OperandValue(fj * fk);
    } else if (rs.op == OpType::FDIV_D) {
        double fj = to_fp(rs.Vj.value());
        double fk = to_fp(rs.Vk.value());
        if (fk == 0.0) return OperandValue(std::numeric_limits<double>::quiet_NaN());
        return OperandValue(fj / fk);
    }
    throw std::runtime_error("Unsupported FP mul/div op");
}

static int get_rob_index(const std::string& tag) {
    if (tag.empty()) return -1;
    return std::stoi(tag.substr(3));
}

void ReservationStation::clear() {
    busy = false;
    op = OpType::UNKNOWN;
    Qj = "";
    Qk = "";
    dest = std::monostate{};
    ROB_idx = -1;
    A = 0;
}

void FunctionalUnit::start(OpType _op, const OperandValue& a, const OperandValue& b, 
               int _rob_idx, const std::string& _rs_type, int _rs_idx) {
        op = _op;
        v1 = a;
        v2 = b;
        rob_idx = _rob_idx;
        rs_type = _rs_type;
        rs_idx = _rs_idx;
        remaining_cycles = get_latency(_op);
        busy = true;
    }

void FunctionalUnit::clear() {
    busy = false;
    remaining_cycles = 0;
    op = OpType::UNKNOWN;
    v1 = OperandValue{}, v2 = OperandValue{};
    rob_idx = -1;
    rs_type.clear();
    rs_idx = -1;
}

void ROBEntry::clear() {
    busy = false;
    op = OpType::UNKNOWN;
    dest = std::monostate{};
    is_load = false;
    is_store = false;
    result;
    state = InstructionState::ISSUED;

    lsq_idx = -1;
}

OperandValue FunctionalUnit::compute_result() const {
        ReservationStation fake_rs;
        fake_rs.op = op;
        fake_rs.Vj = v1;
        fake_rs.Vk = v2;

        if (rs_type == "INTALU") return execute_alu_op(fake_rs);
        if (rs_type == "MULDIV") return execute_muldiv_op(fake_rs);
        if (rs_type == "FPADD") return execute_fp_add_op(fake_rs);
        if (rs_type == "FPMUL" || rs_type == "FPDIV") return execute_fp_mul_op(fake_rs);
        // LOAD/STORE执行后计算
        return OperandValue(0ULL);
    }

bool issue_instruction(const Instruction& instr) {
    if (rob_count >= ROB_SIZE) return false;
    std::cout<< instr.toString() <<std::endl;

    int rob_idx = rob_tail;
    rob[rob_idx] = ROBEntry{
        .busy = true,
        .op = instr.op,
        .dest = get_dest_reg_from_instruction(instr),
        .is_load = is_load_op(instr.op),
        .is_store = is_store_op(instr.op),
        .state = InstructionState::ISSUED,
        .lsq_idx = -1,
        .instr = instr
    };

    bool issued = false;

    // --- ALU 指令 ---
    if (!is_load_op(instr.op) && !is_store_op(instr.op)) {
        ReservationStation* target_rs = nullptr;
        int rs_size = 0;
        if (is_alu_op(instr.op)) {
            target_rs = intalu_rs;
            rs_size = NUM_INTALU_RS;
        } else if (is_muldiv_op(instr.op)) {
            target_rs = muldiv_rs;
            rs_size = NUM_MULDIV_RS;
        } else if (is_fp_add_op(instr.op)) {
            target_rs = fpadd_rs;
            rs_size = NUM_FPADD_RS;
        } else if (is_fp_mul_op(instr.op)) {
            target_rs = fpmul_rs;
            rs_size = NUM_FPMUL_RS;
        } else if (is_fp_div_op(instr.op)) {
            target_rs = fpdiv_rs;
            rs_size = NUM_FPDIV_RS;
        }

        if (target_rs) {
            for (int i = 0; i < rs_size; ++i) {
                if (!target_rs[i].busy) {
                    target_rs[i].clear();
                    target_rs[i].busy = true;
                    target_rs[i].op = instr.op;
                    target_rs[i].ROB_idx = rob_idx;
                    target_rs[i].A = instr.imm;

                    // rs1 → Vj/Qj
                    if (instr.rs1 >= 0) {
                        IntReg src_reg(instr.rs1);
                        if (regs_int_status[src_reg.idx] == "") {
                            std::cout << " Vj " << regs_int[src_reg.idx];
                            target_rs[i].Vj = OperandValue(regs_int[src_reg.idx]);
                        } else {
                            std::cout << " Qj " << regs_int_status[src_reg.idx];
                            int dep_rob_idx = get_rob_index(regs_int_status[src_reg.idx]);
                            if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                                target_rs[i].Vj = rob[dep_rob_idx].result;
                            } else {
                                target_rs[i].Qj = regs_int_status[src_reg.idx];
                            }
                        }
                    } else if (instr.fs1 >= 0) {
                        FpReg src_reg(instr.fs1);
                        if (regs_fp_status[src_reg.idx] == "") {
                            target_rs[i].Vj = OperandValue(regs_fp[src_reg.idx]);
                        } else {
                            int dep_rob_idx = get_rob_index(regs_fp_status[src_reg.idx]);
                            if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                                target_rs[i].Vj = rob[dep_rob_idx].result;
                            } else {
                                target_rs[i].Qj = regs_fp_status[src_reg.idx];
                            }
                        }
                    }

                    // rs2 or imm → Vk/Qk
                    if (instr.rs2 >= 0) {
                        IntReg src_reg(instr.rs2);
                        if (regs_int_status[src_reg.idx] == "") {
                            target_rs[i].Vk = OperandValue(regs_int[src_reg.idx]);
                        } else {
                            int dep_rob_idx = get_rob_index(regs_int_status[src_reg.idx]);
                            if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                                target_rs[i].Vk = rob[dep_rob_idx].result;
                            } else {
                                target_rs[i].Qk = regs_int_status[src_reg.idx];
                            }
                        }
                    } else if (instr.fs2 >= 0) {
                        FpReg src_reg(instr.fs2);
                        if (regs_fp_status[src_reg.idx] == "") {
                            target_rs[i].Vk = OperandValue(regs_fp[src_reg.idx]);
                        } else {
                            int dep_rob_idx = get_rob_index(regs_fp_status[src_reg.idx]);
                            if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                                target_rs[i].Vk = rob[dep_rob_idx].result;
                            } else {
                                target_rs[i].Qk = regs_fp_status[src_reg.idx];
                            }
                        }
                    } else {
                        // I-type: use immediate
                        target_rs[i].Vk = OperandValue(static_cast<uint64_t>(static_cast<int64_t>(instr.imm)));
                    }

                    issued = true;
                    break;
                }
            }
        }
    }
    // --- Load 指令 ---
    else if (is_load_op(instr.op)) {
        if (lsq_count >= LSQ_SIZE) return false;

        int rs_idx = -1;
        for (int i = 0; i < NUM_LOAD_RS; ++i) {
            if (!load_rs[i].busy) {
                rs_idx = i;
                break;
            }
        }
        if (rs_idx == -1) return false;

        int lsq_idx = lsq_tail;
        lsq[lsq_idx] = LSQEntry{
            .valid = true,
            .is_store = false,
            .op = instr.op,
            .rob_idx = rob_idx,
            .dest = get_dest_reg_from_instruction(instr)
        };
        rob[rob_idx].lsq_idx = lsq_idx;

        auto& rs = load_rs[rs_idx];
        rs.busy = true;
        rs.op = instr.op;
        rs.ROB_idx = rob_idx;
        rs.A = instr.imm;

        if (instr.rs1 >= 0) {
            IntReg src_reg(instr.rs1);
            if (regs_int_status[src_reg.idx] == "") {
                rs.Vj = OperandValue(regs_int[src_reg.idx]);
            } else {
                int dep_rob_idx = get_rob_index(regs_int_status[src_reg.idx]);
                if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                    rs.Vj = rob[dep_rob_idx].result;
                } else {
                    rs.Qj = regs_int_status[src_reg.idx];
                }
            }
        } else if (instr.fs1 >= 0) {
            FpReg src_reg(instr.fs1);
            if (regs_fp_status[src_reg.idx] == "") {
                rs.Vj = OperandValue(regs_fp[src_reg.idx]);
            } else {
                int dep_rob_idx = get_rob_index(regs_fp_status[src_reg.idx]);
                if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                    rs.Vj = rob[dep_rob_idx].result;
                } else {
                    rs.Qj = regs_fp_status[src_reg.idx];
                }
            }
        }

        lsq_tail = (lsq_tail + 1) % LSQ_SIZE;
        lsq_count++;
        issued = true;
    }
    // --- Store 指令 ---
    else if (is_store_op(instr.op)) {
        if (lsq_count >= LSQ_SIZE) return false;

        int rs_idx = -1;
        for (int i = 0; i < NUM_STORE_RS; ++i) {
            if (!store_rs[i].busy) {
                rs_idx = i;
                break;
            }
        }
        if (rs_idx == -1) return false;

        int lsq_idx = lsq_tail;
        lsq[lsq_idx] = LSQEntry{
            .valid = true,
            .is_store = true,
            .op = instr.op,
            .rob_idx = rob_idx
        };
        rob[rob_idx].lsq_idx = lsq_idx;

        auto& rs = store_rs[rs_idx];
        rs.busy = true;
        rs.op = instr.op;
        rs.ROB_idx = rob_idx;
        rs.A = instr.imm;

        if (instr.rs1 >= 0) {
            IntReg src_reg(instr.rs1);
            if (regs_int_status[src_reg.idx] == "") {
                rs.Vj = OperandValue(regs_int[src_reg.idx]);
            } else {
                int dep_rob_idx = get_rob_index(regs_int_status[src_reg.idx]);
                if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                    rs.Vj = rob[dep_rob_idx].result;
                } else {
                    rs.Qj = regs_int_status[src_reg.idx];
                }
            }
        } else if (instr.fs1 >= 0) {
            FpReg src_reg(instr.fs1);
            if (regs_fp_status[src_reg.idx] == "") {
                rs.Vj = OperandValue(regs_fp[src_reg.idx]);
            } else {
                int dep_rob_idx = get_rob_index(regs_fp_status[src_reg.idx]);
                if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                    rs.Vj = rob[dep_rob_idx].result;
                } else {
                    rs.Qj = regs_fp_status[src_reg.idx];
                }
            }
        }

        if (instr.rs2 >= 0) {
            IntReg data_reg(instr.rs2);
            if (regs_int_status[data_reg.idx] == "") {
                rs.Vk = OperandValue(regs_int[data_reg.idx]);
            } else {
                int dep_rob_idx = get_rob_index(regs_int_status[data_reg.idx]);
                if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                    rs.Vk = rob[dep_rob_idx].result;
                } else {
                    rs.Qk = regs_int_status[data_reg.idx];
                }
            }
        } else if (instr.fs2 >= 0) {
            FpReg data_reg(instr.fs2);
            if (regs_fp_status[data_reg.idx] == "") {
                rs.Vk = OperandValue(regs_fp[data_reg.idx]);
            } else {
                int dep_rob_idx = get_rob_index(regs_fp_status[data_reg.idx]);
                if (dep_rob_idx >= 0 && rob[dep_rob_idx].state == InstructionState::EXECUTED) {
                    rs.Vk = rob[dep_rob_idx].result;
                } else {
                    rs.Qk = regs_fp_status[data_reg.idx];
                }
            }
        }

        lsq_tail = (lsq_tail + 1) % LSQ_SIZE;
        lsq_count++;
        issued = true;
    }

    std::visit([&](const auto& dest_reg) {
        using T = std::decay_t<decltype(dest_reg)>;
        if constexpr (std::is_same_v<T, IntReg>) {
            regs_int_status[dest_reg.idx] = "ROB" + std::to_string(rob_idx);
        } else if constexpr (std::is_same_v<T, FpReg>) {
            regs_fp_status[dest_reg.idx] = "ROB" + std::to_string(rob_idx);
        }
    }, rob[rob_idx].dest);

    if (issued) {
        rob_tail = (rob_tail + 1) % ROB_SIZE;
        rob_count++;
        return true;
    }
    return false;
}

void executeFU() {
    cdb_list.clear();
    // --- 启动新操作 ---
    auto try_launch_to_fu = [&](auto& fu_array, const std::string& rs_type,
                                int rs_size, ReservationStation* rs_array) {
        for (int i = 0; i < rs_size; ++i) {
            auto& rs = rs_array[i];
            if (!rs.busy) continue;
            if (!rs.Qj.empty()) continue;
            if (rs_type != "LOAD" && (!rs.Qk.empty() || !rs.Vk)) continue;

            if (rob[rs.ROB_idx].state >= InstructionState::EXECUTING) continue;

            // 找一个空闲 FU
            for (auto& fu : fu_array) {
                if (!fu.busy) {
                    OperandValue v1 = *rs.Vj;
                    OperandValue v2 = (rs_type == "LOAD") ? OperandValue(0.0) : *rs.Vk;
                    fu.clear();
                    fu.start(rs.op, v1, v2, rs.ROB_idx, rs_type, i);
                    rob[rs.ROB_idx].state = InstructionState::EXECUTING;
                    break;
                }
            }
        }
    };

    try_launch_to_fu(int_alu_fus, "INTALU", NUM_INTALU_RS, intalu_rs);
    try_launch_to_fu(int_muldiv_fu, "MULDIV", NUM_MULDIV_RS, muldiv_rs);
    try_launch_to_fu(load_fus, "LOAD", NUM_LOAD_RS, load_rs);
    try_launch_to_fu(store_fus, "STORE", NUM_STORE_RS, store_rs);
    try_launch_to_fu(fp_add_fus, "FPADD", NUM_FPADD_RS, fpadd_rs);
    try_launch_to_fu(fp_mul_fus, "FPMUL", NUM_FPMUL_RS, fpmul_rs);
    try_launch_to_fu(fp_div_fu, "FPDIV", NUM_FPDIV_RS, fpdiv_rs);

    auto process_fu_array = [&](auto& fu_array, const std::string& rs_type_base) {
        for (auto& fu : fu_array) {
            if (!fu.busy) continue;

            fu.remaining_cycles--;
            if (fu.remaining_cycles == 0) {
                // 执行完成！
                bool is_store = (rs_type_base == "STORE");
                bool is_load  = (rs_type_base == "LOAD");

                // 获取对应的 RS
                auto get_rs_ptr = [&](int idx) -> ReservationStation* {
                    if (rs_type_base == "INTALU") return &intalu_rs[idx];
                    if (rs_type_base == "MULDIV") return &muldiv_rs[idx];
                    if (rs_type_base == "LOAD") return &load_rs[idx];
                    if (rs_type_base == "STORE") return &store_rs[idx];
                    if (rs_type_base == "FPADD") return &fpadd_rs[idx];
                    if (rs_type_base == "FPMUL") return &fpmul_rs[idx];
                    if (rs_type_base == "FPDIV") return &fpdiv_rs[idx];
                    return nullptr;
                };

                ReservationStation* rs = get_rs_ptr(fu.rs_idx);

                if (is_load) {
                    // Load: 地址 = v1 + A（但 A 存在 RS 中！）
                    // 所以我们仍需从 RS 读取 A
                    if (rs && rs->busy) {
                        uint64_t addr = to_int(fu.v1) + rs->A;
                        OperandValue result;
                        if (rs->op == OpType::LW || rs->op == OpType::LD) {
                            result = OperandValue(memory_int.count(addr) ? memory_int[addr] : 0ULL);
                        } else {
                            result = OperandValue(memory_fp.count(addr) ? memory_fp[addr] : 0.0);
                        }
                        rob[fu.rob_idx].result = result;

                        if (rob[fu.rob_idx].lsq_idx != -1) {
                            lsq[rob[fu.rob_idx].lsq_idx].address = addr;
                            lsq[rob[fu.rob_idx].lsq_idx].addr_ready = true;
                        }
                        std::string rob_tag = "ROB" + std::to_string(fu.rob_idx);
                        cdb_list.push_back(CDB{rob_tag, result});
                        rob[fu.rob_idx].state = InstructionState::EXECUTED;
                    }
                } else if (is_store) {
                    if (rs && rs->busy) {
                        uint64_t addr = to_int(fu.v1) + rs->A;
                        if (rob[fu.rob_idx].lsq_idx != -1) {
                            lsq[rob[fu.rob_idx].lsq_idx].address = addr;
                            lsq[rob[fu.rob_idx].lsq_idx].addr_ready = true;
                            lsq[rob[fu.rob_idx].lsq_idx].data = fu.v2;
                        }
                        rob[fu.rob_idx].state = InstructionState::EXECUTED;
                    }
                } else {
                    // ALU / MUL / FP
                    OperandValue result = fu.compute_result();
                    rob[fu.rob_idx].result = result;
                    std::string rob_tag = "ROB" + std::to_string(fu.rob_idx);
                    cdb_list.push_back(CDB{rob_tag, result});
                    rob[fu.rob_idx].state = InstructionState::EXECUTED;

                    if (rs->op == OpType::BNE) {
                        // BNE: 跳转偏移 = imm / 2 (RISC-V 规范)
                        uint64_t sign = to_int(result);
                        if(sign == 1)
                            next_fetch_branch = next_fetch_idx - 1 + int64_t(rs->A / 4);

                        std::cout<<"sign="<<sign << "\t pc=" << int64_t(rs->A) << "\tbranch\t" << next_fetch_branch <<std::endl;
                    }
                    else if (rs->op == OpType::JALR) {
                        // JALR: 目标地址 = rs1 + imm
                        uint64_t rs1_val = to_int(result);
                        uint64_t target_addr = rs1_val + rs->A;
                        next_fetch_branch = target_addr / 4; // 转换为指令索引
                    }
                }

                // 释放 RS
                if (rs) {
                    rs->busy = false;
                }

                // 释放 FU
                fu.busy = false;
            }
        }
    };

    // 推进所有功能单元
    process_fu_array(int_alu_fus, "INTALU");
    process_fu_array(int_muldiv_fu, "MULDIV");
    process_fu_array(load_fus, "LOAD");
    process_fu_array(store_fus, "STORE");
    process_fu_array(fp_add_fus, "FPADD");
    process_fu_array(fp_mul_fus, "FPMUL");
    process_fu_array(fp_div_fu, "FPDIV");
}

int times = 0;

void commit_head_of_rob() {
    if (rob_count == 0) return;
    int idx = rob_head;
    ROBEntry& entry = rob[idx];

    
    if (entry.state != InstructionState::EXECUTED) {
        times++;
        if(times == 6)
            std::cout<<"\n\n TO DEBUG\n\n";
        if(times >= 16)
        std::exit(1);
        return;
    }
    times = 0;

    // 只有 EXECUTED 的指令才能提交（Load 在 execute 阶段已写 result）
    if (entry.state != InstructionState::EXECUTED) {
        return;
    }

    // Store: 在提交时才写内存
    if (entry.is_store) {
        if (entry.lsq_idx == -1) {
            goto release_rob;
        }
        LSQEntry& lsq_entry = lsq[entry.lsq_idx];
        if (!lsq_entry.addr_ready || !lsq_entry.data.has_value()) {
            return; // 地址或数据未就绪（理论上 execute 后应就绪）
        }
        uint64_t addr = lsq_entry.address;
        const OperandValue& data = *lsq_entry.data;

        if (entry.op == OpType::SW || entry.op == OpType::SD) {
            memory_int[addr] = to_int(data);
        } else if (entry.op == OpType::FSD) {
            memory_fp[addr] = to_fp(data);
        }

        // 标记 LSQ 条目为无效
        lsq_entry.valid = false;
        lsq_count--;
    }
    // Load 或 ALU 指令：写回寄存器文件
    else if (entry.is_load || (!entry.is_store)) {
        if (entry.result.has_value()) {
            std::visit([&](const auto& dest_reg) {
                using T = std::decay_t<decltype(dest_reg)>;
                if constexpr (std::is_same_v<T, IntReg>) {
                    regs_int[dest_reg.idx] = to_int(*entry.result);
                    std::string current_tag = regs_int_status[dest_reg.idx];
                    std::string my_tag = "ROB" + std::to_string(rob_head);
                    if (current_tag == my_tag) {
                        regs_int_status[dest_reg.idx] = "";
                    }
                } else if constexpr (std::is_same_v<T, FpReg>) {
                    regs_fp[dest_reg.idx] = to_fp(*entry.result);
                    std::string current_tag = regs_fp_status[dest_reg.idx];
                    std::string my_tag = "ROB" + std::to_string(rob_head);
                    if (current_tag == my_tag) {
                        regs_fp_status[dest_reg.idx] = "";
                    }
                }
            }, entry.dest);
        }
    }

    std::cout<<int(entry.op)<<std::endl;

release_rob:
    // 提交完成，释放 ROB 条目
    entry.busy = false;
    entry.state = InstructionState::COMMITTED;
    rob_head = (rob_head + 1) % ROB_SIZE;
    rob_count--;

    // 如果是 Load，也要释放 LSQ 条目
    if (entry.is_load && entry.lsq_idx != -1) {
        lsq[entry.lsq_idx].valid = false;
        lsq_count--;
    }
}

// --- CDB 广播 ---
void CDB_broadcast() {
    for (const auto& cdb : cdb_list) {
        auto broadcast_to_rs = [&](ReservationStation& rs) {
            if (rs.Qj == cdb.producer_id) {
                rs.Vj = cdb.value;
                rs.Qj.clear();
            }
            if (rs.Qk == cdb.producer_id) {
                rs.Vk = cdb.value;
                rs.Qk.clear();
            }
        };

        for (int i = 0; i < NUM_INTALU_RS; ++i) broadcast_to_rs(intalu_rs[i]);
        for (int i = 0; i < NUM_MULDIV_RS; ++i) broadcast_to_rs(muldiv_rs[i]);
        for (int i = 0; i < NUM_LOAD_RS; ++i) broadcast_to_rs(load_rs[i]);
        for (int i = 0; i < NUM_STORE_RS; ++i) broadcast_to_rs(store_rs[i]);
        for (int i = 0; i < NUM_FPADD_RS; ++i) broadcast_to_rs(fpadd_rs[i]);
        for (int i = 0; i < NUM_FPMUL_RS; ++i) broadcast_to_rs(fpmul_rs[i]);
        for (int i = 0; i < NUM_FPDIV_RS; ++i) broadcast_to_rs(fpdiv_rs[i]);
    }
}

void print_cycle_state(int cycle) {
    std::cout << "\n========== CYCLE " << cycle << " ==========\n";
    if(cycle == 7)
        std::cout<<"\n";

    // --- Print ROB (only non-COMMITTED or busy entries) ---
    bool rob_printed_header = false;
    for (int i = 0; i < ROB_SIZE; ++i) {
        // 只打印未提交的条目（包括 ISSUED, EXECUTED）
        if (!rob[i].busy) {
            continue;
        }
        if (!rob_printed_header) {
            std::cout << "ROB (head=" << rob_head << ", tail=" << rob_tail << ", count=" << rob_count << "):\n";
            rob_printed_header = true;
        }

        std::string state_str;
        switch (rob[i].state) {
            case InstructionState::ISSUED: state_str = "ISSUED"; break;
            case InstructionState::EXECUTING: state_str = "EXECUTING"; break;
            case InstructionState::EXECUTED: state_str = "EXECUTED"; break;
            case InstructionState::COMMITTED: state_str = "COMMITTED"; break;
            default: state_str = "UNKNOWN";
        }

        std::string dest_str = std::visit([](const auto& r) -> std::string {
            using T = std::decay_t<decltype(r)>;
            if constexpr (std::is_same_v<T, IntReg>) return "x" + std::to_string(r.idx);
            else if constexpr (std::is_same_v<T, FpReg>) return "f" + std::to_string(r.idx);
            else return "-";
        }, rob[i].dest);

        std::cout << "  ROB" << i << " : op=" << static_cast<int>(rob[i].op)
                  << " instr="<< rob[i].instr.toString()
                  << " dest=" << dest_str
                  << " state=" << state_str
                  << " lsq_idx=" << rob[i].lsq_idx
                  << (rob[i].result.has_value() ? " [has result]" : "")
                  << "\n";
    }
    
    // --- Print Register Status ---
    std::cout << "\nInteger Register Status:\n";
    for (int i = 0; i < 32; ++i) {
        if (!regs_int_status[i].empty()) {
            std::cout << "  x" << i << " <- " << regs_int_status[i] << "\n";
        }
    }
    std::cout << "FP Register Status:\n";
    for (int i = 0; i < 32; ++i) {
        if (!regs_fp_status[i].empty()) {
            std::cout << "  f" << i << " <- " << regs_fp_status[i] << "\n";
        }
    }
    std::cout << "\nInteger Register value:\n";
    for (int i = 0; i < 32; ++i) {
        if (regs_int[i] != 0) {
            std::cout << "  x" << i << " <- " << static_cast<int64_t>(regs_int[i]) << "\t";
        }
    }
    std::cout << "\nFP Register value:\n";
    for (int i = 0; i < 32; ++i) {
        if (regs_fp[i] != 0.0) {
            std::cout << "  f" << i << " <- " << static_cast<double>(regs_fp[i]) <<  "\t";
        }
    }
    std::cout<<std::endl;

    // --- Print Reservation Stations ---
    // 已经只打印 busy 的，保持不变
    auto print_rs_array = [](const std::string& name, ReservationStation* rs, int size) {
        bool printed_header = false;
        for (int i = 0; i < size; ++i) {
            if (rs[i].busy) {
                if (!printed_header) {
                    std::cout << "\n" << name << ":\n";
                    printed_header = true;
                }
                std::cout << "  " << name << i << ": op=" << static_cast<int>(rs[i].op)
                          << " ROB" << rs[i].ROB_idx
                          << " Qj=" << (rs[i].Qj.empty() ? "-" : rs[i].Qj)
                          << " Qk=" << (rs[i].Qk.empty() ? "-" : rs[i].Qk);
                if (rs[i].Vj) std::cout << " Vj=" << format_operand_value(*rs[i].Vj);
                if (rs[i].Vk) std::cout << " Vk=" << format_operand_value(*rs[i].Vk);
                std::cout << " A=" << rs[i].A << "\n";
            }
        }
        // 可选：不打印空 RS
    };

    print_rs_array("INTALU_RS", intalu_rs, NUM_INTALU_RS);
    print_rs_array("MULDIV_RS", muldiv_rs, NUM_MULDIV_RS);
    print_rs_array("LOAD_RS", load_rs, NUM_LOAD_RS);
    print_rs_array("STORE_RS", store_rs, NUM_STORE_RS);
    print_rs_array("FPADD_RS", fpadd_rs, NUM_FPADD_RS);
    print_rs_array("FPMUL_RS", fpmul_rs, NUM_FPMUL_RS);
    print_rs_array("FPDIV_RS", fpdiv_rs, NUM_FPDIV_RS);

    // --- Print CDB broadcasts this cycle ---
    if (!cdb_list.empty()) {
        std::cout << "\nCDB Broadcasts:\n";
        for (const auto& cdb : cdb_list) {
            std::cout << "  " << cdb.producer_id << " -> ";
            try {
                uint64_t iv = to_int(cdb.value);
                std::cout << iv;
            } catch (...) {
                double dv = to_fp(cdb.value);
                std::cout << dv;
            }
            std::cout << "\n";
        }
    }

    std::cout << "========================================\n\n";
}

void simulate(const std::vector<Instruction>& instructions,
    const MemoryInitData& mem_init,
    const RegisterInitData& reg_init, 
    bool ENABLE_CYCLE_PRINT) {
    // 初始化全局状态
    for (int i = 0; i < 32; ++i) {
        regs_int[i] = 0;
        regs_fp[i] = 0.0;
        regs_int_status[i] = "";
        regs_fp_status[i] = "";
    }
    for (const auto& [idx, val] : reg_init.int_regs) {
        if (idx >= 0 && idx < 32) {
            if (idx == 0) continue;   // x0 is hardwired to 0
            regs_int[idx] = val;
        }
    }
    for (const auto& [idx, val] : reg_init.fp_regs) {
        if (idx >= 0 && idx < 32) {
            regs_fp[idx] = val;
        }
    }

    memory_int.clear();
    memory_fp.clear();

    for (const auto& [addr, val] : mem_init.int_data) {
        memory_int[addr] = val;
    }
    for (const auto& [addr, val] : mem_init.fp_data) {
        memory_fp[addr] = val;
    }

    // 清空保留站
    auto clear_rs_array = [](ReservationStation* arr, int size) {
        for (int i = 0; i < size; ++i) {
            arr[i] = ReservationStation{};
        }
    };
    clear_rs_array(intalu_rs, NUM_INTALU_RS);
    clear_rs_array(muldiv_rs, NUM_MULDIV_RS);
    clear_rs_array(load_rs, NUM_LOAD_RS);
    clear_rs_array(store_rs, NUM_STORE_RS);
    clear_rs_array(fpadd_rs, NUM_FPADD_RS);
    clear_rs_array(fpmul_rs, NUM_FPMUL_RS);
    clear_rs_array(fpdiv_rs, NUM_FPDIV_RS);

    // 清空功能单元
    auto clear_fu_array = [](auto& arr) {
        for (auto& fu : arr) {
            fu.clear();
        }
    };
    clear_fu_array(int_alu_fus);
    clear_fu_array(load_fus);
    clear_fu_array(store_fus);
    clear_fu_array(fp_add_fus);
    clear_fu_array(fp_mul_fus);
    clear_fu_array(int_muldiv_fu);
    clear_fu_array(fp_div_fu);

    // 清空 ROB 和 LSQ
    for (int i = 0; i < ROB_SIZE; ++i) {
        rob[i] = ROBEntry{};
    }
    for (int i = 0; i < LSQ_SIZE; ++i) {
        lsq[i] = LSQEntry{};
    }
    rob_head = rob_tail = rob_count = 0;
    lsq_head = lsq_tail = lsq_count = 0;

    instruction_queue = instructions;
    next_fetch_idx = 0;

    int cycle = 0;
    // 模拟直到所有指令都取完且 ROB 为空
    while (next_fetch_idx < instruction_queue.size() || rob_count > 0) {
        std::cout<<rob_count<<std::endl;
        // 3. Commit 阶段：提交 ROB 头部（按序提交）
        commit_head_of_rob();
        // 2. Execute & Broadcast 阶段
        executeFU();

        // 无分支延迟槽（执行后立即更新）
        std::cout<<"next_fetch_branch="<<next_fetch_branch<<std::endl;
        if(next_fetch_branch != next_fetch_idx) {
            next_fetch_idx = next_fetch_branch;
            std::cout << " pc=" << next_fetch_idx << std::endl;
        }
        // 1. Issue 阶段：按序发射（这里简化为每周期 1 条）
        if (next_fetch_idx < instruction_queue.size()) {
            std::cout<< instruction_queue[next_fetch_idx].toString() <<std::endl;
            if(issue_instruction(instruction_queue[next_fetch_idx]))
                next_fetch_idx++;
        }

        CDB_broadcast();
        next_fetch_branch = next_fetch_idx;

        if(ENABLE_CYCLE_PRINT)
            print_cycle_state(cycle);
        cycle++;
    }
}