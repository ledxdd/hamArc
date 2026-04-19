#include "hamarc/hamarc.h"
#include <iostream>
#include "additional/additional.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: hamarc -f <archive> <command> [files...]" << std::endl;
        return 1;
    }
    auto args = ParseArguments(argc, argv);
    HamArc arc(args.archive_name);
    ExecuteCommand(arc, args.command, args.files);
    return 0;
}