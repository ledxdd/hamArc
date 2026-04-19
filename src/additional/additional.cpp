#include "additional.h"

bool IsCommand(const std::string& arg) {
    return arg == "-c" || arg == "--create" || arg == "-l" || arg == "--list" || 
           arg == "-x" || arg == "--extract" || arg == "-a" || arg == "--append" || 
           arg == "-d" || arg == "--delete" || arg == "-A" || arg == "--concatenate";
}

void ParseArchiveName(ParsedArguments& args, const std::string& arg, int& i, int argc, char* argv[]) {
    if (arg.substr(0, 7) == "--file=") {
        args.archive_name = arg.substr(7);
    } else if (arg.size() > 2) {
        args.archive_name = arg.substr(2);
    } else if (i + 1 < argc) {
        args.archive_name = argv[++i];
    }
}

ParsedArguments ParseArguments(int argc, char* argv[]) {
    ParsedArguments args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "-f" || arg.substr(0, 7) == "--file=") {
            ParseArchiveName(args, arg, i, argc, argv);
        } else if (IsCommand(arg)) {
            args.command = arg;
        } else {
            args.files.push_back(arg);
        }
    }
    return args;
}

void ExecuteCommand(HamArc& arc, const std::string& command, const std::vector<std::string>& files) {
    if (command == "-c" || command == "--create") {
        arc.Create(files);
    } else if (command == "-l" || command == "--list") {
        arc.List();
    } else if (command == "-x" || command == "--extract") {
        arc.Extract(files);
    } else if (command == "-a" || command == "--append") {
        arc.Append(files);
    } else if (command == "-d" || command == "--delete") {
        arc.Delete(files);
    } else if (command == "-A" || command == "--concatenate") {
        arc.Concatenate(files[0]);
    }
}
