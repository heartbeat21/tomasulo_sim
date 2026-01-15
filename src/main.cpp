#include "instruction.h"
#include "tomasulo_sim.h"
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>

std::vector<Instruction> load_instructions_from_bin(const std::string& filename);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <program.bin>\n";
        return 1;
    }

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

    // 寄存器初始化
    RegisterInitData reg_init;
    reg_init.int_regs = {
        {5, start_addr},   // R1 = x1 = 0x1040 (起始地址)
        {6, end_addr}      // R2 = x2 = 0x1000 (终止地址)
    };
    reg_init.fp_regs = {
        {2, 2.0}           // F2 = 2.0 (乘数)
    };

    try {
        auto instructions = load_instructions_from_bin(argv[1]);
        simulate(instructions, mem_init, reg_init, true);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}