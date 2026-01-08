// src/loader.cpp
#include "instruction.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

extern Instruction decode_instruction(uint32_t inst_word);

std::vector<Instruction> load_instructions_from_bin(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(file), {});
    file.close();

    std::vector<Instruction> instructions;
    size_t num_inst = buffer.size() / 4;

    for (size_t i = 0; i < num_inst; ++i) {
        uint32_t word = static_cast<uint32_t>(buffer[i*4]) |
                       (static_cast<uint32_t>(buffer[i*4+1]) << 8) |
                       (static_cast<uint32_t>(buffer[i*4+2]) << 16) |
                       (static_cast<uint32_t>(buffer[i*4+3]) << 24);

        Instruction inst = decode_instruction(word);
        // 可选：跳过 unknown 指令
        // if (inst.op != OpType::UNKNOWN)
        instructions.push_back(inst);
    }

    return instructions;
}