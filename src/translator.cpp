// src/main.cpp
#include "instruction.h"
#include <vector>
#include <iostream>
#include <iomanip>

std::vector<Instruction> load_instructions_from_bin(const std::string& filename);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <program.bin>\n";
        return 1;
    }

    try {
        auto instructions = load_instructions_from_bin(argv[1]);
        std::cout << "Loaded " << instructions.size() << " instructions:\n";
        for (size_t i = 0; i < instructions.size(); ++i) {
            std::cout << "[" << std::setw(2) << i << "] "
                      << "0x" << std::hex << std::setfill('0') << std::setw(8) << instructions[i].raw
                      << " : " << instructions[i].toString() << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}