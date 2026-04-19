#include <string>
#include <vector>
#include "../hamarc/hamarc.h"

bool IsCommand(const std::string& arg);

struct ParsedArguments {
    std::string archive_name;
    std::string command;
    std::vector<std::string> files;
};

void ParseArchiveName(ParsedArguments& args, const std::string& arg, int& i, int argc, char* argv[]);

ParsedArguments ParseArguments(int argc, char* argv[]);

void ExecuteCommand(HamArc& arc, const std::string& command, const std::vector<std::string>& files);
