#pragma once

#include <cstdint>

struct HammingHeader {
    char magic[8];
    uint32_t version;
    uint32_t file_count;
    uint64_t table_offset;
    uint64_t table_size;
    char reserved[32];
};

struct FileInfo {
    char name[80];
    uint64_t original_size;
    uint64_t offset;
    uint64_t encoded_size;
    uint8_t padding[7];
};