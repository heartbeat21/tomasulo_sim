// src/tomasulo_sim.h
#ifndef TOMASULO_SIM_H
#define TOMASULO_SIM_H
# include <vector>
# include <string>
# include <unordered_map>
# include <iostream>
# include <iomanip>
# include <cassert>
# include <variant>
# include <optional>
# include <queue>
# include "instruction.h"

// 全局模拟器状态

// 各种保留站数目
const int NUM_INTALU_RS = 6;
const int NUM_MULDIV_RS = 2;
const int NUM_LOAD_RS   = 8;
const int NUM_STORE_RS  = 6;
const int NUM_FPADD_RS  = 4;
const int NUM_FPMUL_RS  = 4;
const int NUM_FPDIV_RS  = 2;

// 功能单元数量
const int NUM_INT_ALUS = 2;
const int NUM_LOAD_UNITS = 2;
const int NUM_FP_ADDERS = 2;
const int NUM_FP_MULTIPLIERS = 2;

// ROB条目数
const int ROB_SIZE = 32;

// LSQ
const int LSQ_SIZE = 16; 

// 支持类型
using OperandValue = std::variant<uint64_t, double>;

// 目的寄存器标签类型
struct IntReg {
    int idx;
    IntReg(int i) : idx(i) { assert(i >= 0 && i < 32); }
};
struct FpReg {
    int idx;
    FpReg(int i) : idx(i) { assert(i >= 0 && i < 32); }
};

// 目的寄存器：无 / 整数 / 浮点
using DestReg = std::variant<std::monostate, IntReg, FpReg>;

enum class InstructionState {
    ISSUED,
    EXECUTING,
    EXECUTED,
    COMMITTED
};

// 寄存器
extern uint64_t regs_int[32];
extern double regs_fp[32];

// 寄存器状态表
extern std::string regs_int_status[32];
extern std::string regs_fp_status[32];

// 内存模型
extern std::unordered_map<uint64_t, uint64_t> memory_int;
extern std::unordered_map<uint64_t, double> memory_fp;

// 保留站
struct ReservationStation {
    bool busy = false;
    OpType op = OpType::UNKNOWN;

    std::string Qj = "";
    std::optional<OperandValue> Vj;
    std::string Qk = "";
    std::optional<OperandValue> Vk;

    DestReg dest = std::monostate{};

    int ROB_idx = -1;
    int64_t A = 0;

    void clear();
};

extern ReservationStation intalu_rs[NUM_INTALU_RS];
extern ReservationStation muldiv_rs[NUM_MULDIV_RS];
extern ReservationStation load_rs[NUM_LOAD_RS];
extern ReservationStation store_rs[NUM_STORE_RS];
extern ReservationStation fpadd_rs[NUM_FPADD_RS];
extern ReservationStation fpmul_rs[NUM_FPMUL_RS];
extern ReservationStation fpdiv_rs[NUM_FPDIV_RS];

// 功能单元
struct FunctionalUnit {
    bool busy = false;
    int remaining_cycles = 0;
    OpType op = OpType::UNKNOWN;
    OperandValue v1{}, v2{};
    int rob_idx = -1;
    std::string rs_type; // "INTALU", "FPADD", etc.
    int rs_idx = -1;

    void start(OpType _op, const OperandValue& a, const OperandValue& b,
               int _rob_idx, const std:: string& _rs_type, int _rs_idx);
    void clear();
    OperandValue compute_result() const;
};

extern std::array<FunctionalUnit, NUM_INT_ALUS> int_alu_fus;
extern std::array<FunctionalUnit, NUM_LOAD_UNITS> load_fus;
extern std::array<FunctionalUnit, NUM_FP_ADDERS> fp_add_fus;
extern std::array<FunctionalUnit, NUM_FP_MULTIPLIERS> fp_mul_fus;
extern std::array<FunctionalUnit, 1> store_fus;
extern std::array<FunctionalUnit, 1> int_muldiv_fu;
extern std::array<FunctionalUnit, 1> fp_div_fu;

// ROB
struct ROBEntry {
    bool busy = false;
    OpType op = OpType::UNKNOWN;
    DestReg dest = std::monostate{};
    bool is_load = false;
    bool is_store = false;
    std::optional<OperandValue> result;
    InstructionState state = InstructionState::ISSUED;

    int lsq_idx = -1;
    // debug
    Instruction instr;

    void clear();
};

extern ROBEntry rob[ROB_SIZE];
extern int rob_head;
extern int rob_tail;
extern int rob_count;

// CDB
struct CDB {
    std::string producer_id = "";
    OperandValue value;
};
extern CDB cdb;

// LSQ
struct LSQEntry {
    bool valid = false;
    bool is_store = false;          // true: store, false: load
    OpType op = OpType::UNKNOWN;

    uint64_t address = 0;
    bool addr_ready = false;

    std::optional<OperandValue> data; // for store: data to write; for load: result

    int rob_idx = -1;
    DestReg dest = std::monostate{};
    bool committed = false;
};

extern LSQEntry lsq[LSQ_SIZE];
extern int lsq_head;
extern int lsq_tail;
extern int lsq_count;

extern std::vector<Instruction> instruction_queue;
extern size_t next_fetch_idx;

std::string get_rs_id(const std::string& type, int idx);
bool is_alu_op(OpType op);
bool is_muldiv_op(OpType op);
bool is_load_op(OpType op);
bool is_store_op(OpType op);
bool is_fp_add_op(OpType op);
bool is_fp_mul_op(OpType op);
bool is_fp_div_op(OpType op);
int get_latency(OpType op);

inline uint64_t to_int(const OperandValue& v) {
    if (auto* i = std::get_if<uint64_t>(&v)) return *i;
    throw std::runtime_error("Expected integer in OperandValue");
}
inline double to_fp(const OperandValue& v) {
    if (auto* f = std::get_if<double>(&v)) return *f;
    throw std::runtime_error("Expected double in OperandValue");
}

// memory and regs
struct RegisterInitData {
    std::vector<std::pair<int, uint64_t>> int_regs;   // {reg_idx, value}
    std::vector<std::pair<int, double>> fp_regs;      // {reg_idx, value}
};

struct MemoryInitData {
    std::vector<std::pair<uint64_t, uint64_t>> int_data;
    std::vector<std::pair<uint64_t, double>> fp_data;
};


void simulate(const std::vector<Instruction>& instructions, const MemoryInitData& mem_init = {}, const RegisterInitData& reg_init = {}, bool ENABLE_CYCLE_PRINT = false);
#endif