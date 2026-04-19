#pragma once

#include <iostream>
#include "../types.h"
#include "hamarc.h"
#include "../hamming/hamming.h"

#include <fstream>

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