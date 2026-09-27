#include "writer.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path path = argv[1];
    std::filesystem::remove(path);
    ccs::runtime::Writer writer(path);
    for (int i = 0; i < 128; ++i) {
        writer.write(std::to_string(i));
        if (!writer.flush() || writer.drain_state() != ccs::runtime::Writer::DrainState::Complete) return 1;
        std::ifstream input(path);
        std::string line;
        int count = 0;
        while (std::getline(input, line)) {
            if (line != std::to_string(count++)) return 1;
        }
        if (count != i + 1) {
            std::cerr << "flush returned before the write completed\n";
            return 1;
        }
    }
    const auto invalid_path = path.parent_path() / "writer-directory";
    std::filesystem::create_directories(invalid_path);
    ccs::runtime::Writer failed_writer(invalid_path);
    if (!failed_writer.write("cannot write into a directory") || failed_writer.flush()) return 1;
    if (failed_writer.drain_state() != ccs::runtime::Writer::DrainState::Failed) return 1;
    std::filesystem::remove(invalid_path);
}
