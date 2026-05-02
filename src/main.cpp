#include <iostream>
#include <string>

#ifndef PROJOT_VERSION
#define PROJOT_VERSION "0.0.0"
#endif

int main(int argc, char* argv[]) {
    if (argc > 1) {
        const std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::cout << "projot " << PROJOT_VERSION << "\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: projot <subcommand> [options]\n";
            std::cout << "Run 'projot <subcommand> --help' for subcommand options.\n";
            return 0;
        }
    }
    std::cerr << "projot: no subcommand specified. Run 'projot --help' for usage.\n";
    return 1;
}
