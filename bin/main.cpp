#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <ios>
#include <iterator>

uint8_t CalculateParityBits(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p4 = d2 ^ d3 ^ d4;
    return (p1 << 1) | (p2 << 2) | (p4 << 4);
}

uint8_t CalculateOverallParity(uint8_t code) {
    uint8_t overall = 0;
    while (code) {
        overall ^= code & 1;
        code >>= 1;
    }
    return overall;
}

uint8_t EncodeNibble(uint8_t nibble) {
    uint8_t d1 = (nibble >> 0) & 1;
    uint8_t d2 = (nibble >> 1) & 1;
    uint8_t d3 = (nibble >> 2) & 1;
    uint8_t d4 = (nibble >> 3) & 1;

    uint8_t parity = CalculateParityBits(d1, d2, d3, d4);
    uint8_t code = parity | (d1 << 3) | (d2 << 5) | (d3 << 6) | (d4 << 7);
    
    return code | CalculateOverallParity(code);
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

struct DecodedBits {
    uint8_t p0, p1, p2, d1, p4, d2, d3, d4;
};

DecodedBits ExtractBits(uint8_t code) {
    DecodedBits bits{};
    bits.p0 = (code >> 0) & 1;
    bits.p1 = (code >> 1) & 1;
    bits.p2 = (code >> 2) & 1;
    bits.d1 = (code >> 3) & 1;
    bits.p4 = (code >> 4) & 1;
    bits.d2 = (code >> 5) & 1;
    bits.d3 = (code >> 6) & 1;
    bits.d4 = (code >> 7) & 1;
    return bits;
}

uint8_t CalculateSyndrome(const DecodedBits& bits) {
    uint8_t s1 = bits.p1 ^ bits.d1 ^ bits.d2 ^ bits.d4;
    uint8_t s2 = bits.p2 ^ bits.d1 ^ bits.d3 ^ bits.d4;
    uint8_t s4 = bits.p4 ^ bits.d2 ^ bits.d3 ^ bits.d4;
    return (s4 << 2) | (s2 << 1) | s1;
}

uint8_t BuildNibble(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    return (d4 << 3) | (d3 << 2) | (d2 << 1) | d1;
}

int DecodeNibble(uint8_t code, uint8_t& corrected) {
    DecodedBits bits = ExtractBits(code);
    uint8_t syndrome = CalculateSyndrome(bits);
    if (syndrome) code ^= (1 << (syndrome - 1));
    bits = ExtractBits(code);
    corrected = BuildNibble(bits.d1, bits.d2, bits.d3, bits.d4);
    return bits.p0 ^ bits.p1 ^ bits.p2 ^ bits.d1 ^ bits.p4 ^ bits.d2 ^ bits.d3 ^ bits.d4;
}

uint8_t DecodeBytePair(uint8_t low_code, uint8_t high_code) {
    uint8_t low = 0, high = 0;
    DecodeNibble(low_code, low);
    DecodeNibble(high_code, high);
    return (high << 4) | low;
}

std::vector<uint8_t> Decode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.reserve(data.size() / 2);
    for (size_t i = 0; i < data.size(); i += 2) {
        out.push_back(DecodeBytePair(data[i], data[i + 1]));
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

HamArc::HamArc(std::string archive_name) : archive_name_(std::move(archive_name)) {}

void HamArc::WriteHeader(std::ostream& array, const HammingHeader& header) {
    array.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

HammingHeader HamArc::ReadHeader(std::istream& array) {
    HammingHeader header;
    array.read(reinterpret_cast<char*>(&header), sizeof(header));
    return header;
}

std::string GetFileName(const FileInfo& file) {
    return std::string(file.name, strnlen(file.name, sizeof(file.name)));
}

std::string HamArc::GetFileName(const FileInfo& file) {
    return GetFileName(file);
}

std::vector<uint8_t> ReadFileContent(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

FileInfo CreateFileInfo(const std::string& path, uint64_t offset, size_t original_size, size_t encoded_size) {
    FileInfo file{};
    std::string name = path.substr(path.find_last_of("/") + 1);
    std::strncpy(file.name, name.c_str(), 79);
    file.original_size = original_size;
    file.offset = offset;
    file.encoded_size = encoded_size;
    return file;
}

FileInfo HamArc::EncodeAndWriteFile(const std::string& path, std::ostream& array, uint64_t& data_offset) {
    auto raw = ReadFileContent(path);
    auto encoded = Encode(raw);
    
    FileInfo file = CreateFileInfo(path, data_offset, raw.size(), encoded.size());
    
    array.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    data_offset += encoded.size();
    return file;
}

HammingHeader CreateDefaultHeader() {
    HammingHeader header{};
    std::memcpy(header.magic, "HAMARC", 6);
    header.version = 1;
    return header;
}

void WriteFileData(std::ostream& out, std::vector<FileInfo>& table, const std::vector<std::vector<uint8_t>>& blocks) {
    uint64_t data_offset = sizeof(HammingHeader);
    for (size_t i = 0; i < table.size(); ++i) {
        table[i].offset = data_offset;
        out.write(reinterpret_cast<const char*>(blocks[i].data()), blocks[i].size());
        data_offset += blocks[i].size();
    }
}

void WriteFileTable(std::ostream& out, const std::vector<FileInfo>& table) {
    for (const auto& file : table) {
        out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    }
}

void HamArc::RewriteArchive(std::vector<FileInfo> table, const std::vector<std::vector<uint8_t>>& blocks) {
    std::ofstream out(archive_name_, std::ios::binary | std::ios::trunc);
    
    HammingHeader header = CreateDefaultHeader();
    WriteHeader(out, header);
    
    WriteFileData(out, table, blocks);
    
    uint64_t table_offset = out.tellp();
    WriteFileTable(out, table);
    
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

std::vector<uint8_t> ReadEncodedFile(std::istream& array, const FileInfo& file) {
    array.seekg(file.offset);
    std::vector<uint8_t> enc(file.encoded_size);
    array.read(reinterpret_cast<char*>(enc.data()), file.encoded_size);
    return enc;
}

bool HamArc::ExtractSingleFile(std::istream& array, const FileInfo& file) {
    auto enc = ReadEncodedFile(array, file);
    auto dec = Decode(enc);
    std::ofstream out(GetFileName(file), std::ios::binary);
    out.write(reinterpret_cast<const char*>(dec.data()), dec.size());
    return true;
}

void HamArc::Create(const std::vector<std::string>& files) {
    std::ofstream array(archive_name_, std::ios::binary | std::ios::trunc);
    
    HammingHeader header = CreateDefaultHeader();
    WriteHeader(array, header);
    
    uint64_t data_offset = sizeof(HammingHeader);
    std::vector<FileInfo> table;
    for (const auto& file : files) {
        table.push_back(EncodeAndWriteFile(file, array, data_offset));
    }
    
    uint64_t table_offset = array.tellp();
    WriteFileTable(array, table);
    
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

bool ShouldExtractFile(const FileInfo& file, const std::vector<std::string>& files) {
    return files.empty() || std::find(files.begin(), files.end(), GetFileName(file)) != files.end();
}

bool HamArc::Extract(const std::vector<std::string>& files) {
    std::ifstream array(archive_name_, std::ios::binary);
    auto table = LoadFileTable(array);

    for (const auto& file : table) {
        if (ShouldExtractFile(file, files)) {
            ExtractSingleFile(array, file);
        }
    }
    return true;
}

uint64_t CalculateAppendOffset(const std::vector<FileInfo>& table) {
    return table.back().offset + table.back().encoded_size;
}

bool HamArc::Append(const std::vector<std::string>& files) {
    std::fstream array(archive_name_, std::ios::binary | std::ios::in | std::ios::out);
    auto table = LoadFileTable(array);
    
    uint64_t data_offset = CalculateAppendOffset(table);
    array.seekp(data_offset);
    
    for (const auto& file : files) {
        table.push_back(EncodeAndWriteFile(file, array, data_offset));
    }

    RewriteArchive(std::move(table), {});
    return true;
}

std::vector<uint8_t> ReadFileBlock(std::istream& stream, const FileInfo& file) {
    stream.seekg(file.offset);
    std::vector<uint8_t> data(file.encoded_size);
    stream.read(reinterpret_cast<char*>(data.data()), data.size());
    return data;
}

void CollectFilesFromArchive(std::istream& stream, std::vector<FileInfo>& table, std::vector<std::vector<uint8_t>>& blocks) {
    for (const auto& file : table) {
        blocks.push_back(ReadFileBlock(stream, file));
    }
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

    table = table1;
    CollectFilesFromArchive(src_this, table1, blocks);
    
    table.insert(table.end(), table2.begin(), table2.end());
    CollectFilesFromArchive(src_other, table2, blocks);

    RewriteArchive(std::move(table), blocks);
    return true;
}

bool ShouldDeleteFile(const FileInfo& file, const std::vector<std::string>& files_to_delete) {
    std::string name = GetFileName(file);
    return std::find(files_to_delete.begin(), files_to_delete.end(), name) != files_to_delete.end();
}

bool HamArc::Delete(const std::vector<std::string>& files_to_delete) {
    std::ifstream array(archive_name_, std::ios::binary);
    auto old_table = LoadFileTable(array);

    std::vector<FileInfo> new_table;
    std::vector<std::vector<uint8_t>> new_blocks;
    new_table.reserve(old_table.size());
    new_blocks.reserve(old_table.size());

    for (const auto& file : old_table) {
        if (ShouldDeleteFile(file, files_to_delete)) {
            continue;
        }
        new_table.push_back(file);
        new_blocks.push_back(ReadFileBlock(array, file));
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
        if (arg.substr(0, 7) == "--file=") {
            args.archive_name = arg.substr(7);
        } else if (arg.substr(0, 2) == "-f") {
            args.archive_name = arg.size() > 2 ? arg.substr(2) : argv[++i];
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
