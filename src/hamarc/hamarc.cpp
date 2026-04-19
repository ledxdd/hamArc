#include "hamarc.h"

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
    return GetFileName(file);
}

std::vector<uint8_t> ReadFileContent(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

FileInfo CreateFileInfo(const std::string& path, uint64_t offset, size_t original_size, size_t encoded_size) {
    FileInfo file{};
    std::string name = path.substr(path.find_last_of("/") + 1);
    if (name.size() > 79) name.resize(79);
    std::strncpy(file.name, name.c_str(), 79);
    file.name[79] = '\0';
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

std::string GetFileName(const FileInfo& file) {
    return std::string(file.name);
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
    return table.empty() ? sizeof(HammingHeader) : table.back().offset + table.back().encoded_size;
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
    return std::find(files_to_delete.begin(), files_to_delete.end(), GetFileName(file)) != files_to_delete.end();
}

bool HamArc::Delete(const std::vector<std::string>& files_to_delete) {
    std::ifstream array(archive_name_, std::ios::binary);
    auto old_table = LoadFileTable(array);
    std::vector<FileInfo> new_table;
    std::vector<std::vector<uint8_t>> new_blocks;

    new_table.reserve(old_table.size());
    new_blocks.reserve(old_table.size());

    for (const auto& file : old_table) {
        if (ShouldDeleteFile(file, files_to_delete)) continue;
        new_table.push_back(file);
        new_blocks.push_back(ReadFileBlock(array, file));
    }
    RewriteArchive(std::move(new_table), new_blocks);
    return true;
}