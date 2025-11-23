#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <ios>
#include <stdexcept>
#include <iterator>

uint8_t EncodeNibble(uint8_t nibble) {
    uint8_t d1 = (nibble >> 0) & 1;
    uint8_t d2 = (nibble >> 1) & 1;
    uint8_t d3 = (nibble >> 2) & 1;
    uint8_t d4 = (nibble >> 3) & 1;

    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p4 = d2 ^ d3 ^ d4;

    uint8_t code = (p1 << 1) | (p2 << 2) | (d1 << 3) | (p4 << 4) | (d2 << 5) | (d3 << 6) | (d4 << 7);

    uint8_t overall = 0;
    uint8_t temp = code;
    while (temp) { 
        overall ^= temp & 1; temp >>= 1;
    }

    return code | (overall << 0);
}

std::vector<uint8_t> Encode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        out.push_back(EncodeNibble(byte & 0x0F));
        out.push_back(EncodeNibble(byte >> 4));
    }
    return out;
}

int DecodeNibble(uint8_t code, uint8_t& corrected) {
    uint8_t p0 = (code >> 0) & 1;
    uint8_t p1 = (code >> 1) & 1;
    uint8_t p2 = (code >> 2) & 1;
    uint8_t d1 = (code >> 3) & 1;
    uint8_t p4 = (code >> 4) & 1;
    uint8_t d2 = (code >> 5) & 1;
    uint8_t d3 = (code >> 6) & 1;
    uint8_t d4 = (code >> 7) & 1;

    uint8_t s1 = p1 ^ d1 ^ d2 ^ d4;
    uint8_t s2 = p2 ^ d1 ^ d3 ^ d4;
    uint8_t s4 = p4 ^ d2 ^ d3 ^ d4;
    uint8_t syndrome = (s4 << 2) | (s2 << 1) | s1;

    uint8_t overall = p0 ^ p1 ^ p2 ^ d1 ^ p4 ^ d2 ^ d3 ^ d4;

    if (syndrome == 0) {
        corrected = (d4 << 3) | (d3 << 2) | (d2 << 1) | d1;
        return (overall == 0) ? 0 : 1;
    }

    if (overall == 1) {
        int pos = syndrome;
        code ^= (1 << (pos - 1));

        d1 = (code >> 3) & 1;
        d2 = (code >> 5) & 1;
        d3 = (code >> 6) & 1;
        d4 = (code >> 7) & 1;

        corrected = (d4 << 3) | (d3 << 2) | (d2 << 1) | d1;
        return 1;
    }

    return -1;
}

std::vector<uint8_t> Decode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.reserve(data.size() / 2);

    for (size_t i = 0; i < data.size(); i += 2) {
        uint8_t low = 0, high = 0;
        int e1 = DecodeNibble(data[i], low);
        int e2 = DecodeNibble(data[i + 1], high);
        if (e1 == -1 || e2 == -1) {
            throw std::runtime_error("2 mistakes");
		}
        out.push_back((high << 4) | low);
    }
    return out;
}

struct HammingHeader {
    char magic[8] {};
    uint32_t version {};
    uint32_t file_count {};
    uint64_t table_offset {};
    uint64_t table_size {};
    char reserved[32] {};
};

struct FileInfo {
    char name[80] {};
    uint64_t original_size {};
    uint64_t offset {};
    uint64_t encoded_size {};
    uint8_t flags {};
    uint8_t padding[7] {};
};

class HamArc {
public:
    HamArc(std::string archive_name);

    void Create(const std::vector<std::string>& files);
    void List();
    bool Extract(const std::vector<std::string>& files = {});
    bool Append(const std::vector<std::string>& files);
    bool Concatenate(const std::string& other_archive);
    bool Delete(const std::vector<std::string>& files_to_delete);

private:
    std::string archive_name_;

    static void WriteHeader(std::ostream& array, const HammingHeader& header);
    static HammingHeader ReadHeader(std::istream& array);
    static std::string GetFileName(const FileInfo& file);

    FileInfo EncodeAndWriteFile(const std::string& path, std::ostream& array, uint64_t& data_offset);
    void RewriteArchive(std::vector<FileInfo> table, const std::vector<std::vector<uint8_t>>& blocks);

    std::vector<FileInfo> LoadFileTable(std::istream& array);
    bool ExtractSingleFile(std::istream& array, const FileInfo& file);
};

HamArc::HamArc(std::string archive_name) : archive_name_(std::move(archive_name)){
    if (archive_name_.size() < 4 || archive_name_.substr(archive_name_.size() - 4) != ".haf") {
		archive_name_ += ".haf";
	}
}

void HamArc::WriteHeader(std::ostream& array, const HammingHeader& header) {
    array.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

HammingHeader HamArc::ReadHeader(std::istream& array) {
    HammingHeader header;
    array.read(reinterpret_cast<char*>(&header), sizeof(header));
    return header;
}

std::string HamArc::GetFileName(const FileInfo& file) {
    return std::string(file.name, strnlen(file.name, sizeof(file.name)));
}

FileInfo HamArc::EncodeAndWriteFile(const std::string& path, std::ostream& array, uint64_t& data_offset) {
    std::ifstream in(path, std::ios::binary);

    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), {});

    FileInfo file{};
    std::string name = path.substr(path.find_last_of("/") + 1);
    if (name.size() > 79) name.resize(79);
    std::strncpy(file.name, name.c_str(), 79);
    file.name[79] = '\0';

    auto encoded = Encode(raw);
    file.original_size = raw.size();
    file.offset = data_offset;
    file.encoded_size = encoded.size();

    array.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    data_offset += encoded.size();
    return file;
}

void HamArc::RewriteArchive(std::vector<FileInfo> table, const std::vector<std::vector<uint8_t>>& blocks) {
    std::ofstream out(archive_name_, std::ios::binary | std::ios::trunc);

    HammingHeader header{};
    std::memcpy(header.magic, "HAMARC", 6);
    header.version = 1;
    WriteHeader(out, header);

    uint64_t data_offset = sizeof(HammingHeader);
    for (size_t i = 0; i < table.size(); ++i) {
        table[i].offset = data_offset;
        out.write(reinterpret_cast<const char*>(blocks[i].data()), blocks[i].size());
        data_offset += blocks[i].size();
    }

    uint64_t table_offset = out.tellp();
    for (const auto& file : table)
        out.write(reinterpret_cast<const char*>(&file), sizeof(file));

    header.file_count = static_cast<uint32_t>(table.size());
    header.table_offset = table_offset;
    header.table_size = table.size() * sizeof(FileInfo);

    out.seekp(0);
    WriteHeader(out, header);
}

std::vector<FileInfo> HamArc::LoadFileTable(std::istream& array) {
    HammingHeader header = ReadHeader(array);
    array.seekg(header.table_offset);

    std::vector<FileInfo> table(header.file_count);
    for (auto& file : table)
        array.read(reinterpret_cast<char*>(&file), sizeof(file));
    return table;
}

bool HamArc::ExtractSingleFile(std::istream& array, const FileInfo& file) {
    array.seekg(file.offset);
    std::vector<uint8_t> enc(file.encoded_size);
    array.read(reinterpret_cast<char*>(enc.data()), file.encoded_size);

    std::vector<uint8_t> dec = Decode(enc);

    std::string out_name = GetFileName(file);
    std::ofstream out(out_name, std::ios::binary);
    out.write(reinterpret_cast<const char*>(dec.data()), dec.size());
    return !!out;
}

void HamArc::Create(const std::vector<std::string>& files) {
    std::ofstream array(archive_name_, std::ios::binary | std::ios::trunc);

    HammingHeader header{};
    std::memcpy(header.magic, "HAMARC", 6);
    header.version = 1;
    WriteHeader(array, header);

    uint64_t data_offset = sizeof(HammingHeader);
    std::vector<FileInfo> table;

    for (const auto& file : files)
        table.push_back(EncodeAndWriteFile(file, array, data_offset));

    uint64_t table_offset = array.tellp();
    for (const auto& file : table)
        array.write(reinterpret_cast<const char*>(&file), sizeof(file));

    header.file_count = static_cast<uint32_t>(table.size());
    header.table_offset = table_offset;
    header.table_size = table.size() * sizeof(FileInfo);
    array.seekp(0);
    WriteHeader(array, header);
}

void HamArc::List() {
    std::ifstream array(archive_name_, std::ios::binary);
    HammingHeader header = ReadHeader(array);
    array.seekg(header.table_offset);

    for (uint32_t i = 0; i < header.file_count; ++i) {
        FileInfo file{};
        array.read(reinterpret_cast<char*>(&file), sizeof(file));
        std::cout << GetFileName(file);
    }
}

bool HamArc::Extract(const std::vector<std::string>& files) {
    std::ifstream array(archive_name_, std::ios::binary);

    auto table = LoadFileTable(array);

    for (const auto& file : table) {
        std::string name = GetFileName(file);
        if (!files.empty() && std::find(files.begin(), files.end(), name) == files.end())
            continue;

        ExtractSingleFile(array, file);
    }
    return true;
}

bool HamArc::Append(const std::vector<std::string>& files) {
    std::fstream array(archive_name_, std::ios::binary | std::ios::in | std::ios::out);

    HammingHeader header = ReadHeader(array);
    array.seekg(header.table_offset);

    std::vector<FileInfo> table(header.file_count);
    for (auto& file : table) {
		array.read(reinterpret_cast<char*>(&file), sizeof(file));
	}

    uint64_t data_offset = sizeof(HammingHeader);
    if (!table.empty())
        data_offset = table.back().offset + table.back().encoded_size;

    array.seekp(data_offset);
    for (const auto& file : files)
        table.push_back(EncodeAndWriteFile(file, array, data_offset));

    RewriteArchive(std::move(table), {});
    return true;
}

bool HamArc::Concatenate(const std::string& other_archive) {
    std::ifstream src_this(archive_name_, std::ios::binary);
    std::ifstream src_other(other_archive, std::ios::binary);

    auto table1 = LoadFileTable(src_this);
    auto table2 = LoadFileTable(src_other);

    std::vector<FileInfo> table;
    std::vector<std::vector<uint8_t>> blocks;

    table.reserve(table1.size() + table2.size());
    blocks.reserve(table1.size() + table2.size());

    for (const auto& file : table1) {
        src_this.seekg(file.offset);
        std::vector<uint8_t> data(file.encoded_size);
        src_this.read(reinterpret_cast<char*>(data.data()), data.size());
        table.push_back(file);
        blocks.push_back(std::move(data));
    }
    for (const auto& file : table2) {
        src_other.seekg(file.offset);
        std::vector<uint8_t> data(file.encoded_size);
        src_other.read(reinterpret_cast<char*>(data.data()), data.size());
        table.push_back(file);
        blocks.push_back(std::move(data));
    }

    RewriteArchive(std::move(table), blocks);
    return true;
}

bool HamArc::Delete(const std::vector<std::string>& files_to_delete) {
    std::ifstream array(archive_name_, std::ios::binary);

    auto old_table = LoadFileTable(array);

    std::vector<FileInfo> new_table;
    std::vector<std::vector<uint8_t>> new_blocks;
    new_table.reserve(old_table.size());
    new_blocks.reserve(old_table.size());

    for (const auto& file : old_table) {
        std::string name = GetFileName(file);
        if (std::find(files_to_delete.begin(), files_to_delete.end(), name) != files_to_delete.end()) {
            continue;
        }

        array.seekg(file.offset);
        std::vector<uint8_t> data(file.encoded_size);
        array.read(reinterpret_cast<char*>(data.data()), data.size());

        new_table.push_back(file);
        new_blocks.push_back(std::move(data));
    }

    RewriteArchive(std::move(new_table), new_blocks);
    return true;
}

struct ParsedArguments {
    std::string archive_name;
    std::string command;
    std::vector<std::string> files;
};

ParsedArguments ParseArguments(int argc, char* argv[]) {
    ParsedArguments args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "-f" || arg.substr(0, 7) == "--file=") {
            if (arg.substr(0, 7) == "--file=") {
                args.archive_name = arg.substr(7);
            } else if (arg.size() > 2) {
                args.archive_name = arg.substr(2);
            } else if (i + 1 < argc) {
                args.archive_name = argv[++i];
            }
        } else if (arg == "-c" || arg == "--create" || arg == "-l" || arg == "--list" || 
                   arg == "-x" || arg == "--extract" || arg == "-a" || arg == "--append" || 
                   arg == "-d" || arg == "--delete" || arg == "-A" || arg == "--concatenate") {
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

int main(int argc, char* argv[]) {
    auto args = ParseArguments(argc, argv);
    HamArc arc(args.archive_name);
    ExecuteCommand(arc, args.command, args.files);
    return 0;
}
