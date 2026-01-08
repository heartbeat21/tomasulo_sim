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

    try {
        auto instructions = load_instructions_from_bin(argv[1]);
        simulate(instructions);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}